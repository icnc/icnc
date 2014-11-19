/*
Copyright (c) 2014, Ellen Porter,
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <iostream>
#include <fstream>
#include <ctime>
#include <string>

#include "raytracer.h"


int step_trace_ray::execute
	( const pair & frame_fragment, ray_tracer & rt ) const {

	int current_frame = frame_fragment.first;
	int current_fragment = frame_fragment.second;

	// Consume
	std::vector<Primitive*> scene;
	std::vector<Primitive*> static_scene = rt.static_scene;
	std::vector<Luminaire*> luminaires = rt.luminaires;
	std::vector<Primitive*> dynamic_scene;
	rt.dynamic_scene.get(current_frame, dynamic_scene);

	scene.insert(scene.end(), static_scene.begin(), static_scene.end());
	scene.insert(scene.end(), dynamic_scene.begin(), dynamic_scene.end());

	std::vector<Point2D*> points;
	rt.pixel_locations.get(pair(current_frame, current_fragment), points);

	// Execute

	double duration;
	std::clock_t start = std::clock();
	std::vector<Pixel*> pixels;

	double invWidth = 1/((double)rt.image_width);
	double invHeight = 1/((double)rt.image_height);
	int smaller = std::min(rt.image_width, rt.image_height);

	double fov = 30;

	double aspectratio = rt.image_width/((double)rt.image_height);
	double angle = tan(3.141592653589793 * 0.5 * fov / 180.0);

	for(int ij = 0; ij < points.size(); ij++) {
		double x = (double)points[ij]->i;
		double y = (double)points[ij]->j;
		double nSamp1D = sqrt(rt.number_pixel_samples);
		double duvSamp = 1.0/nSamp1D;
		Multispectral3D superSample = Multispectral3D();
		for(int i = 0; i < nSamp1D; ++i) {
			for(int j = 0; j < nSamp1D; ++j) {
				double u = x + (i + 0.5 * duvSamp)/smaller;
				double v = y + (j + 0.5 * duvSamp)/smaller;
				double xx = (2 * (u * invWidth) - 1) * angle * aspectratio;
				double yy = (1 - 2 * (v * invHeight)) * angle;
				Vector3D raydir = Vector3D(xx, yy, -1).normalize();
				Ray3D ray = Ray3D(Point3D(), raydir, NULL, 0);
				superSample = superSample + trace(ray, scene, luminaires);
			}
		}
		Multispectral3D rgb = superSample / rt.number_pixel_samples;
		pixels.push_back(new Pixel(points[ij]->i,points[ij]->j,rgb.r,rgb.g,rgb.b));
	}

	duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;

	// Produce
	rt.pixels.put(pair(current_frame, current_fragment), pixels);
	rt.execution_time.put(pair(current_frame, current_fragment), duration);



  return CnC::CNC_Success;
};

/**
 * Composes a single image from image fragments
 */
int step_compose_image::execute
	( const int & frame, ray_tracer & rt ) const {

	int current_frame = frame;


	// Consume - gather the pixel fragments

	Multispectral3D *image = new Multispectral3D[rt.image_width*rt.image_height];
	std::vector<Pixel*> *pixels = new std::vector<Pixel*>[rt.number_fragments];
	std::vector<Primitive*> dynamic_scene;
	rt.dynamic_scene.get(current_frame, dynamic_scene);

	for(int fragment = 0; fragment < rt.number_fragments; fragment++) {
		rt.pixels.get(pair(current_frame, fragment), pixels[fragment] );
		for(int i = 0; i < pixels[fragment].size(); i++) {
			image[pixels[fragment][i]->i+rt.image_width*pixels[fragment][i]->j] =
					Multispectral3D( pixels[fragment][i]);
		}
	}

	// Produce - Once we have our pixels we can let the next frame start computing
	if(current_frame + 1 < rt.number_frames)
		rt.frame.put(current_frame + 1);

	// Execute - compose a single image

	std::stringstream frame_id;
	frame_id << "./frames/frame_" << current_frame << ".ppm";
	std::ofstream ofs(frame_id.str().c_str(), std::ios::out | std::ios::binary);
	ofs << "P6\n" << rt.image_width << " " << rt.image_height << "\n255\n";
	for(int i = 0; i < rt.image_width*rt.image_height; i++) {
			ofs <<
			(unsigned char)(std::min(1.0, image[i].r) * 255) <<
			(unsigned char)(std::min(1.0, image[i].g) * 255) <<
			(unsigned char)(std::min(1.0, image[i].b) * 255);
	}

	ofs.close();

	// Clean up
	for(int fragment = 0; fragment < rt.number_fragments; fragment++) {
		for(int i = 0; i < pixels[fragment].size(); i++) {
			delete pixels[fragment][i];
		}
	}
	for(int i = 0; i < dynamic_scene.size(); i++) {
		delete dynamic_scene[i];
	}

	delete [] image;
	delete [] pixels;

  return CnC::CNC_Success;
};

/**
 * Dynamically determines when to rearrange the pixel
 * allocations for the ray tracer step
 */
int step_decompose_image::execute
	( const int & frame, ray_tracer & rt ) const {

	int current_frame = frame;

	// Consume

	// Execute - compute an initial even distribution
	int distributed = 0;
	int current_fragment = -1;

	std::vector<Point2D*> points[rt.number_fragments];

	// No history yet, do an even distribution
	if(current_frame == 0) {
		int distribution = ((double)rt.image_width * (double)rt.image_height) /
				(double)rt.number_fragments;

		for(int i = 0; i < rt.image_width; ++i) {
			for(int j = 0; j < rt.image_height; ++j) {
					if((current_fragment + 1) < rt.number_fragments &&
						distributed % distribution == 0) {
						points[++current_fragment] = std::vector<Point2D*>();
					}
					points[current_fragment].push_back(new Point2D(i, j));
					distributed++;
				}
		}
	} else {
		// Gather our previous pixel locations and times to compute each of them
		std::vector<Point2D*> all_points;
		std::vector<double> average_times;
		double total_time = 0;

		for(int fragment = 0; fragment < rt.number_fragments; fragment++) {
			std::vector<Point2D*> points;
			double time_to_trace;

			rt.pixel_locations.get(pair(current_frame - 1, fragment), points);
			rt.execution_time.get(pair(current_frame - 1, fragment), time_to_trace);

			double average_per_pixel = time_to_trace / (double) points.size();

			std::cout << time_to_trace << ", ";

			for(int i = 0; i < points.size(); i++) {
				all_points.push_back(points[i]);
				average_times.push_back(average_per_pixel);
			}

			total_time += time_to_trace;
		}


		bool random_order = true;

		if(random_order) {
			std::vector<double> myVector(average_times.size());
			std::vector<Point2D*> myVector2(all_points.size());
			std::vector<Point2D*>::iterator it2 = all_points.begin();
			int lastPos = 0;
			for(std::vector<double>::iterator it = average_times.begin();
					it != average_times.end(); it2++, it++, lastPos++) {
			    int insertPos = rand() % (lastPos + 1);
			    if (insertPos < lastPos) {
			        myVector[lastPos] = myVector[insertPos];
			        myVector2[lastPos] = myVector2[insertPos];

			    }
			    myVector[insertPos] = *it;
			    myVector2[insertPos] = *it2;
			}
			all_points = std::vector<Point2D*>(myVector2.begin(), myVector2.end());
			average_times = std::vector<double>(myVector.begin(), myVector.end());
		}

		std::cout << "\n";

		int total_points = all_points.size();

		double goal_time = total_time / 8.0;
		double current_time = 0;

		int current_fragment = 0;
		points[current_fragment] = std::vector<Point2D*>();
		double new_total_time = 0;
		for(int i = 0; i < total_points; i++) {

			double time_for_pixel = average_times.back();
			Point2D * point = all_points.back();
			new_total_time += time_for_pixel;

			average_times.pop_back();
			all_points.pop_back();

			if(current_fragment + 1 < rt.number_fragments &&
					(current_time + time_for_pixel) > goal_time) {
					points[++current_fragment] = std::vector<Point2D*>();
					current_time = 0;
			}

			current_time += time_for_pixel;
			points[current_fragment].push_back(point);
		}

	}

	// Produce
	for(int fragment = 0; fragment < rt.number_fragments; fragment++) {
		std::cout << points[fragment].size() << ", ";
		rt.pixel_locations.put(pair(current_frame, fragment), points[fragment]);
		rt.frame_fragment.put(pair(frame, fragment));
	}

	std::cout << "\n";

	// Clean up
	//delete [] points;

  return CnC::CNC_Success;
};

/**
 * Produces a new dynamic scene for every frame
 */
int step_compute_dynamic_scene::execute
		(const int & frame, ray_tracer & rt ) const {

	int current_frame = frame;

	// Consume - nothing to consume

	// Execute

	std::vector<Primitive*> dynamic_scene;
	pushCube(dynamic_scene, (0.1*current_frame));

	// Produce

	rt.dynamic_scene.put(current_frame, dynamic_scene);

  return CnC::CNC_Success;
};

int main( int argc, char* argv[] )
{
    std::cout << "Hello!\n";

    ray_tracer cnc_ray_tracer = ray_tracer();
 //    CnC::debug::trace(cnc_ray_tracer.frame);
 //   CnC::debug::trace(cnc_ray_tracer.frame_fragment);
 //   CnC::debug::trace(cnc_ray_tracer.execution_time);

    cnc_ray_tracer.number_frames = 12;
    cnc_ray_tracer.number_fragments = 8;

    cnc_ray_tracer.image_height = 1080;
    cnc_ray_tracer.image_width = 1920;

    cnc_ray_tracer.number_pixel_samples = 4;
    cnc_ray_tracer.number_light_samples = 4;

    pushStaticScene(cnc_ray_tracer.static_scene);

  	cnc_ray_tracer.luminaires.push_back(new Luminaire(
  			Multispectral3D(0.1, 0.1, 0.1), Sphere(Point3D(0, 20, -30), 3), 4));

  	// Start the graph
    cnc_ray_tracer.frame.put(0);
    cnc_ray_tracer.wait();

    // Delete the static scene
  	for(unsigned i = 0; i < cnc_ray_tracer.static_scene.size(); ++i) {
  		cnc_ray_tracer.static_scene[i]->deleteShape();
  		delete cnc_ray_tracer.static_scene[i];
  	}

  	// Delete the lights
  	for(unsigned i = 0; i < cnc_ray_tracer.luminaires.size(); ++i) {
  	 	delete cnc_ray_tracer.luminaires[i];
  	}

		for(int fragment = 0; fragment < cnc_ray_tracer.number_fragments; fragment++) {
			std::vector<Point2D*> points;
			cnc_ray_tracer.pixel_locations.get(
					pair(cnc_ray_tracer.number_frames - 1, fragment), points);
			for(int i = 0; i < points.size(); i++)
				delete points[i];
		}

    return 0;
}

