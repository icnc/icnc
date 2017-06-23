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

#include <vector>
#include <fstream>
#include <iostream>
#include <string>
#include <cmath>
#include <sstream>

const int MAX_RAY_DEPTH = 10;

class Primitive;

class Point2D {
public:
	int i, j;
	Point2D(int i, int j):i(i), j(j) {};
};

class Pixel {
public:
	int i, j;
	double r, g, b;
	Pixel(int i, int j, double r, double g, double b):i(i),j(j),r(r),g(g),b(b){};
};

class Multispectral3D {
public:
	double r, g, b;
	Multispectral3D():r(0), g(0), b(0){};
	Multispectral3D(double r, double g, double b):r(r), g(g), b(b){};
	Multispectral3D(Pixel *pixel):r(pixel->r), g(pixel->g), b(pixel->b){};

	Multispectral3D operator+(Multispectral3D a) {
		return Multispectral3D(r + a.r, g + a.g, b + a.b);
	}

	Multispectral3D operator*(double f) {
		return Multispectral3D(r * f, g * f, b * f);
	}

	Multispectral3D operator*(Multispectral3D m) {
		return Multispectral3D(r * m.r, g * m.g, b * m.b);
	}

	Multispectral3D operator/(double f) {
		return Multispectral3D(r / f, g / f, b / f);
	}
};

class Vector2D {
public:
	double x, y;
	Vector2D(): x(0), y(0) {};
	Vector2D(double x, double y):x(x), y(y){};

	Vector2D operator+(Vector2D v) {
		return Vector2D(x + v.x, y + v.y);
	}

	Vector2D operator-(Vector2D v) {
		return Vector2D(x - v.x, y - v.y);
	}

	Vector2D operator*(double f) {
		return Vector2D(x * f, y * f);
	}

	Vector2D operator/(double f) {
		return Vector2D(x / f, y / f);
	}

	double dot(Vector2D v) {
		return x * v.x + y * v.y;
	}

	double mag() {
		return sqrt(x * x + y * y);
	}

	double magSqr() {
		return x * x + y * y;
	}

	Vector2D reflect(Vector2D n) {
		return Vector2D(); // TODO
	}

	Vector2D refract(Vector2D n, double eta) {
		return Vector2D(); // TODO
	}
};

class Vector3D {
public:
	double x, y, z;
	Vector3D():x(0), y(0), z(0){};
	Vector3D(double x, double y, double z):x(x), y(y), z(z){};

	Vector3D operator+(Vector3D v) {
		return Vector3D(x + v.x, y + v.y, z + v.z);
	}

	Vector3D operator-(Vector3D v) {
		return Vector3D(x - v.x, y - v.y, z - v.z);
	}

	Vector3D operator-() {
		return Vector3D(-x, -y, -z);
	}

	Vector3D operator*(double f) {
		return Vector3D(x * f, y * f, z * f);
	}

	Vector3D operator/(double f) {
		return Vector3D(x / f, y / f, z / f);
	}

	double dot(Vector3D v) {
		return x * v.x + y * v.y + z * v.z;
	}

	double mag() {
		return sqrt(x * x + y * y + z * z);
	}

	double magSqr() {
		return x * x + y * y + z * z;
	}

	Vector3D reflect(Vector3D n) {
		return (Vector3D(x, y, z) - (n * 2 * dot(n))).normalize();
	}

	bool refract(Vector3D n, double eta, Vector3D *v) {

		double innerValue = 1 - pow(dot(n), 2);
		if(innerValue < 0)
			return false;


		double sq = eta * sqrt(innerValue);
		double innerValue2 = 1 - pow(sq, 2);

		if(innerValue2 < 0)
			return false;

		Vector3D b = ((Vector3D(x, y, z) - (n * dot(n))) * -1).normalize();

		Vector3D left = b * sq;
		Vector3D right = n * sqrt(innerValue2);

		Vector3D tVector = (left - right).normalize();

		v->x = tVector.x;
		v->y = tVector.y;
		v->z = tVector.z;

		return true;
	}

	Vector3D cross(Vector3D v) {
		return Vector3D((y*v.z-v.y*z), -(x * v.z - v.x * z), (x * v.y - v.x * y));
	}

	Vector3D normalize() {
		double m = mag();
		return Vector3D(x/m, y/m, z/m);
	}
};

class Point3D {
public:
	double x, y, z;
	Point3D():x(0), y(0), z(0){};
	Point3D(double x, double y, double z):x(x), y(y), z(z){};

	Point3D operator+(Point3D p) {
		return Point3D(x + p.x, y + p.y, z + p.z);
	}

	Point3D operator+(Vector3D v) {
		return Point3D(x + v.x, y + v.y, z + v.z);
	}

	Point3D operator-(Vector3D v) {
		return Point3D(x - v.x, y - v.y, z - v.z);
	}

	Vector3D operator-(Point3D p) {
		return Vector3D(x - p.x, y - p.y, z - p.z);
	}

	Point3D operator*(double f) {
		return Point3D(x * f, y * f, z * f);
	}

	Point3D operator/(double f) {
		return Point3D(x / f, y / f, z / f);
	}

	double dot(Vector3D v) {
		return x * v.x + y * v.y + z * v.z;
	}
};

class Transform3D {
public:
	double m[4][4];
	Transform3D() {
		for(int i=0; i<4; i++) { for(int j=0; j<4; j++) {m[i][j] = 0;}}
	};
	Transform3D(double t[4][4]) {
		for(int i=0; i<4; i++) { for(int j=0; j<43; j++) {m[i][j] = t[i][j];}}
	};

	Transform3D operator*(Transform3D t) {
		Transform3D r = Transform3D();
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				for(int k = 0; k < 4; k++) {
					r.m[i][j] += m[i][k]*t.m[k][j];
				}
			}
		}
		return r;
	}

	Point3D operator*(Point3D p) {
			Transform3D r = Transform3D();
			Point3D p_out = Point3D();

			p_out.x = m[0][0] * p.x + m[0][1] * p.y + p.z * m[0][2] + m[0][3];
			p_out.y = m[1][0] * p.x + m[1][1] * p.y + p.z * m[1][2] + m[1][3];
			p_out.z = m[2][0] * p.x + m[2][1] * p.y + p.z * m[2][2] + m[2][3];

			return p_out;
		}
};

class Material {
public:
	Multispectral3D ambient, diffuse, specular;
	Multispectral3D emissivity; // Lights
	double shininess,refraction;
	double kReflective, kRefractive; // %
	bool isLight;
	Material(Multispectral3D ambient, Multispectral3D diffuse,
			Multispectral3D specular, double shininess, double refraction,
			double kReflective, double kRefractive):
			ambient(ambient),diffuse(diffuse),specular(specular),shininess(shininess),
			 refraction(refraction), kReflective(kReflective),kRefractive(kRefractive)
			 { isLight = false; };
	Material(Multispectral3D ambient,Multispectral3D emissivity):ambient(ambient),
		emissivity(emissivity) { isLight = true; };
};

class Intersection3D {
public:
	Point3D intersection;
	Primitive *primitive;
	double distance;

	Intersection3D(){};

	Intersection3D(Point3D intersection, Primitive *primitive, double distance):
		intersection(intersection), primitive(primitive), distance(distance) {};

};

class Ray3D {
public:
	Point3D origin;
	Vector3D direction;
	Primitive *primitive;

	int depth;
	Ray3D(Point3D origin, Vector3D direction, Primitive *primitive, int depth):
		origin(origin),direction(direction),primitive(primitive),depth(depth){};
};

class Sphere {
public:
	Point3D center;
	double radius;
	Sphere(Point3D center, double radius):
		center(center), radius(radius) {};

	bool intersect(Ray3D ray, Intersection3D *intersection,Primitive *primitive) {

		double a = ray.direction.dot(ray.direction);
		double b = (ray.origin - center).dot(ray.direction) * 2;
		double c = (ray.origin - center).dot(ray.origin - center) - pow(radius,  2);

		double discriminant = b * b - 4 * a * c;
		if(discriminant < 0)
			return false; // No intersection

		// Solve the positive case
		double t = (-b + sqrt(discriminant))/ (2 * a);

		if(discriminant != 0) { // Need to solve the negative case too
			double tn = (-b - sqrt(discriminant))/ (2 * a);
			if(tn < t)
				t = tn;
		}

		if(t <= 0.0)
			return false; // Backwards

		intersection->distance = t;
		intersection->primitive = primitive;
		intersection->intersection = ray.origin + (ray.direction * t);

		return true;
	}

	Vector3D normal(Point3D point) {
		return (center - point).normalize();
	}
};

class Triangle {
public:
	Point3D o; // origin
	Point3D p; // coutner clockwise from origin
	Point3D q;
	Vector3D n;

	Triangle(Point3D o, Point3D p, Point3D q) : o(o),p(p),q(q){
		n = ((p - o).cross(q - o)).normalize();
	};

	Triangle(Point3D o,Point3D p, Point3D q, Vector3D n) : o(o),p(p),q(q),n(n){ };

	bool intersect(Ray3D ray,Intersection3D *intersection, Primitive *primitive) {

		double t = ray.direction.dot(n);

		if(t == 0)
			return false; // parallel, no intersection

		t = ((p - ray.origin).dot(n))/t;

		Point3D x = ray.origin + (ray.direction * t);

		if((p - o).cross(x - o).dot(n) < 0)
			return false;

		if((q - p).cross(x - p).dot(n) < 0)
			return false;

		if((o - q).cross(x - q).dot(n) < 0)
			return false;

		intersection->distance = t;
		intersection->primitive = primitive;
		intersection->intersection = x;

		return true;
	}

	Vector3D normal(Point3D point) {
		return (p - point).cross(q - point).normalize();
	}
};

class Primitive {
public:
	Material *material;
	Triangle *triangle;
	Sphere *sphere;
	bool isSphere;
	Primitive(Sphere *sphere, Material *material) :
		material(material), sphere(sphere){ isSphere = true; };
	Primitive(Triangle *triangle, Material *material) :
		material(material), triangle(triangle){ isSphere = false; };

	bool intersect(Ray3D ray, Intersection3D *intersection) {
		return isSphere ? sphere->intersect(ray, intersection, this) :
			triangle->intersect(ray, intersection, this);
	}
	Vector3D normal(Point3D point) {
		return isSphere ? sphere->normal(point):(triangle->normal(point) * -1);
	}
	void deleteShape() {
		if(isSphere) { delete sphere; } else { delete triangle; };
	}
};

class Luminaire {
public:
	Multispectral3D irradiance;
	Sphere shape;
	int nLuminairePoints;
	Luminaire(Multispectral3D irradiance, Sphere shape, int nLuminairePoints):
		irradiance(irradiance),shape(shape),nLuminairePoints(nLuminairePoints) {};

	std::vector<Ray3D> shadowRays(Point3D x) {
		std::vector<Ray3D> shadowRays;
		double nSamp1D = sqrt(16);
		double duvSamp = shape.radius/nSamp1D;
		for(int j = 0; j < nSamp1D;j++) {
			for(int k = 0; k < nSamp1D; k++) {
				double x_ = shape.center.x + ((j + 0.5) * duvSamp);
				double z_ = shape.center.z + ((k + 0.5) * duvSamp);
				Point3D p = Point3D(x_,  shape.center.y, z_);
				shadowRays.push_back(Ray3D(p, (x - p).normalize(), NULL, 0));
			}
		}
		return shadowRays;
	 }
};

bool firstIntersection(Ray3D ray, std::vector<Primitive*> &scene,
		Intersection3D *intersection) {
	double near = 1e20;

	bool intersects = false;
	for(unsigned i = 0; i < scene.size(); ++i) {
		Intersection3D temp = Intersection3D();
		if(scene[i]->intersect(ray, &temp)) {
			if(temp.distance < near) {
				intersection->distance = temp.distance;
				intersection->intersection = temp.intersection;
				intersection->primitive = temp.primitive;
				near = temp.distance;
				intersects = true;
			}
		}
	}

	return intersects;
}

Multispectral3D trace(Ray3D ray, std::vector<Primitive*> &scene,
		std::vector<Luminaire*> &luminaires) {
	double near = 1e8;

	Intersection3D intersection = Intersection3D();
	bool intersects = firstIntersection(ray, scene, &intersection);

	if(!intersects) { // No intersection, return background color
		return Multispectral3D(1.0, 1.0, 1.0);
	}

	Material *material = intersection.primitive->material;
	Multispectral3D ambient = material->ambient;
	Point3D x = intersection.intersection;

	// Add a global ambient
	Multispectral3D surfaceColor = material->ambient +
			Multispectral3D(0.05, 0.05, 0.05);

	double bias = 1e-4;
	bool inside = false;

	Vector3D uV = (x - ray.origin).normalize();
	Vector3D uN = intersection.primitive->normal(x);

	if(ray.direction.dot(uN) > 0) uN = -uN, inside = true;

	// Direct lighting
	for(unsigned i = 0; i < luminaires.size(); ++i) {
		Vector3D uS = luminaires[i]->shape.normal(x).normalize();
		Vector3D uR = uS.reflect(uN);
		std::vector<Ray3D> shadowRays = luminaires[i]->shadowRays(x);
		Multispectral3D radianceOfLum = Multispectral3D();
		for(unsigned j = 0; j < shadowRays.size(); ++j) {
			Intersection3D shadowIntersection;
			if(firstIntersection(shadowRays[j], scene, &shadowIntersection)) {
				if(shadowIntersection.primitive == intersection.primitive) {
					double sDotN = uS.dot(uN); // diffuse power
					if(sDotN > 0) {
						Multispectral3D specularContribution = Multispectral3D();
						Multispectral3D diffuseContribution =
								Multispectral3D(material->diffuse * sDotN);
						double rDotV = uR.dot(uV); // specular power
						if(rDotV > 0) {
							double scale = pow(rDotV, material->shininess);
							specularContribution =Multispectral3D(material->specular * scale);
						}
						radianceOfLum = radianceOfLum + (luminaires[i]->irradiance *
							(specularContribution + diffuseContribution));
					}
				}
			}
		}
		surfaceColor = surfaceColor +
				(radianceOfLum * 0.5/((double)luminaires.size()));
	}

	// Indirect lighting
	if((material->kReflective > 0 || material->kRefractive > 0)
			&& ray.depth < MAX_RAY_DEPTH) {
		double facingRatio = -ray.direction.dot(uN);
		double fresnelEffect = 1 * 0.1 + pow(1 - facingRatio, 3) * (1 - 0.1);
		Vector3D refldir = uV.reflect(uN);
		Ray3D reflectedRay = Ray3D(x + (uN * bias), refldir, NULL, ray.depth + 1);
		Multispectral3D reflection = trace(reflectedRay, scene, luminaires);
		Multispectral3D refraction = Multispectral3D();
		if(material->kRefractive > 0) {
			double ior = 1.02;
			double direction = inside ? -1.0 : 1.0;
			double eta = inside ? ior : 1 / ior;
			Vector3D refractedDirection = Vector3D();
			if((-uV).refract((-uN*direction), eta, &refractedDirection)) {
				Ray3D refractedRay = Ray3D(x + (uN * direction) * bias,
						refractedDirection, NULL, ray.depth + 1);
				refraction = trace(refractedRay, scene, luminaires);
			}
		}
		surfaceColor = ((reflection * fresnelEffect) + refraction *
				(1 - fresnelEffect)
			* material->kRefractive) * surfaceColor * material->ambient;
	}

	return surfaceColor;

}


void pushCube(std::vector<Primitive*> &list, double rotation) {

	Material *red_glass = new Material(
	  		Multispectral3D(1.00, 0.32, 0.36),	// ambient
	  		Multispectral3D(1.00, 0.32, 0.36),	// diffuse
	  		Multispectral3D(0.2, 0.2, 0.2),			// specular
	  		7.8974,															// shininess
	  		1.54,																// refraciton index
	  		0.2,																// reflective %
	  		0.6);																// reflective %


	if(1) {
		list.push_back(new Primitive(
				new Sphere(Point3D(0, 0+(rotation), -20), 4), red_glass));
		return;
	}


	Point3D p0 = Point3D(-1, -1, -1);
	Point3D p1 = Point3D(1, -1, -1);
	Point3D p2 = Point3D(-1, -1, 1);
	Point3D p3 = Point3D(1, -1, 1);

	Point3D p4 = Point3D(-1, 1, -1);
	Point3D p5 = Point3D(1, 1, -1);
	Point3D p6 = Point3D(-1, 1, 1);
	Point3D p7 = Point3D(1, 1, 1);

	Point3D translate = Point3D(0, 0, 0);//-15);

	// Rotate the points
	Transform3D transform = Transform3D();

	rotation += 0.001;

	// Rotate about the y
	transform.m[0][0] = cos(rotation);
	transform.m[0][2] = -sin(rotation);
	transform.m[1][1] = 1;
	transform.m[2][0] = sin(rotation);
	transform.m[2][2] = cos(rotation);
	transform.m[3][3] = 1;
	transform.m[2][3] = -15;

/*
 	// Rotate about the z
	transform.m[0][0] = cos(rotation);
	transform.m[0][1] = -sin(rotation);
	transform.m[2][2] = 1;
	transform.m[1][0] = sin(rotation);
	transform.m[1][1] = cos(rotation);
	transform.m[3][3] = 1;
	transform.m[2][3] = -15;
*/

	Point3D p0r = transform * (p0) + translate;
	Point3D p1r = transform * (p1) + translate;
	Point3D p2r = transform * (p2) + translate;
	Point3D p3r = transform * (p3) + translate;
	Point3D p4r = transform * (p4) + translate;
	Point3D p5r = transform * (p5) + translate;
	Point3D p6r = transform * (p6) + translate;
	Point3D p7r = transform * (p7) + translate;


/*
	list.push_back(new Primitive(
			new Sphere(p0r, 1), red_glass));

	list.push_back(new Primitive(
			new Sphere(p1r, 1), red_glass));
*/
	list.push_back(new Primitive(
			new Triangle(p0r, p1r, p2r), red_glass));
	list.push_back(new Primitive(
			new Triangle(p2r, p1r, p3r), red_glass));

	list.push_back(new Primitive(
			new Triangle(p4r, p5r, p6r), red_glass));
	list.push_back(new Primitive(
			new Triangle(p6r, p5r, p7r), red_glass));
/*
	list.push_back(new Primitive(
			new Triangle(p2r, p6r, p3r), red_glass));
	list.push_back(new Primitive(
			new Triangle(p6r, p7r, p3r), red_glass));
*/
	list.push_back(new Primitive(
			new Triangle(p0r, p4r, p6r), red_glass));
	list.push_back(new Primitive(
			new Triangle(p0r, p6r, p2r), red_glass));

	list.push_back(new Primitive(
			new Triangle(p1r, p3r, p5r), red_glass));
	list.push_back(new Primitive(
			new Triangle(p5r, p3r, p7r), red_glass));

	list.push_back(new Primitive(
			new Triangle(p0r, p1r, p4r), red_glass));
	list.push_back(new Primitive(
			new Triangle(p4r, p1r, p5r), red_glass));

}

void pushBunny(std::vector<Primitive*> &list) {


	Material *glass1 = new Material(
	  		Multispectral3D(1.00, 0.32, 0.36),	// ambient
	  		Multispectral3D(1.00, 0.32, 0.36),	// diffuse
	  		Multispectral3D(0.2, 0.2, 0.2),	// specular
	  		7.8974,										// shininess
	  		1.54,											// refraciton index
	  		0.5,											// reflective %
	  		0.5);

	std::vector<Point3D*> points;
	std::vector<Vector3D*> normals;

	std::string line;

	std::ifstream myfile ("bunny.obj");
	int count = 0;

	if (myfile.is_open()) {

		while ( getline (myfile,line) ) {


			std::istringstream iss (line);

			std::string s;

			getline(iss, s, ' ');

			if(s == "v") {
				getline(iss, s, ' ');
/*
				double x = (std::stod(s)) * 100 + 4;

				getline(iss, s, ' ');
				double y = (stod(s)) * 100 - 10;

				getline(iss, s, ' ');
				double z = (stod(s)) * 100 - 40;
				points.push_back(new Point3D(x, y, z));
				*/
			} if(s == "vn") {
				getline(iss, s, ' ');
				/*
				double x = (stod(s));

				getline(iss, s, ' ');
				double y = (stod(s));

				getline(iss, s, ' ');
				double z = (stod(s));

				normals.push_back(new Vector3D(x, y, z));
				*/
			} if(s == "f") {

				getline(iss, s, ' ');
				/*
				string v0 = s.substr(0, s.find('/'));
				string n0 = v0.substr(0, v0.length());

				getline(iss, s, ' ');
				string v1 = s.substr(0, s.find('/'));
				string n1 = v1.substr(0, v1.length());

				getline(iss, s, ' ');
				string v2 = s.substr(0, s.find('/'));
				string n2 = v2.substr(0, v2.length());

				int o = stoi(v0) - 1;
				int p = stoi(v1) - 1;
				int q = stoi(v2) - 1;

				int oN = stoi(n0) - 1;
				int pN = stoi(n1) - 1;
				int qN = stoi(n2) - 1;

				// Calcualte a face normal
				Vector3D normal = (((*normals[oN]) + (*normals[pN]) + (*normals[qN])) / 3).normalize();
				list.push_back(Primitive(new Triangle(*points[o], *points[p], *points[q]), glass1));
	*/
			}

		}

		myfile.close();
	}


	for(int i = 0; i < points.size(); ++i) {
		delete points[i];
	}

	for(int i = 0; i < normals.size(); ++i) {
		delete normals[i];
	}


}


void pushStaticScene(std::vector<Primitive*> &static_scene) {

	Material *mirror = new Material(
		Multispectral3D(.6, .6, .6),	// ambient
		Multispectral3D(.6, .6, .6),	// diffuse
		Multispectral3D(.9, .9, .9),	// specular
		0.8,							// shininess
		1.7,							// refraciton index
		1.0,							// reflective %
		0.1);							// refractive %

	Material *glass = new Material(
		Multispectral3D(0.329412, 0.223529, 0.027451),	// ambient
		Multispectral3D(0.780392, 0.568627, 0.113725),	// diffuse
		Multispectral3D(0.992157, 0.941176, 0.807843),	// specular
		27.8974,										// shininess
		1.54,											// refraciton index
		0.0,											// reflective %
		1.0);											// refractive %

	Material *turquoise = new Material(
		Multispectral3D(0.1, 0.18725, 0.1745),			// ambient
		Multispectral3D(0.396, 0.74151, 0.69102),		// diffuse
		Multispectral3D(0.297254, 0.30829, 0.306678),	// specular
		12.8,											// shininess
		1.61,											// refraciton index
		0.0,											// reflective %
		0.0);											// refractive %

	Material *gold = new Material(
		Multispectral3D(0.24725, 0.1995, 0.0745),		// ambient
		Multispectral3D(0.75164, 0.60648, 0.22648),		// diffuse
		Multispectral3D(0.628281, 0.555802, 0.366065),	// specular
		51.2,											// shininess
		1.44928,										// refraciton index
		0.0,											// reflective %
		0.0);											// refractive %

	Material *turquoise2 = new Material(
		Multispectral3D(0.2, 0.2, 0.2),			// ambient
		Multispectral3D(0.90, 0.90, 0.90),		// diffuse
		Multispectral3D(0.2, 0.2, 0.2),	// specular
		12.8,											// shininess
		1.61,											// refraciton index
		0.0,											// reflective %
		0.0);											// refractive %

	Material *glass1 = new Material(
		Multispectral3D(1.00, 0.32, 0.36),	// ambient
		Multispectral3D(1.00, 0.32, 0.36),	// diffuse
		Multispectral3D(0.2, 0.2, 0.2),	// specular
		7.8974,										// shininess
		1.54,											// refraciton index
		0.5,											// reflective %
		0.5);											// refractive %


	Material *glass2 = new Material(
		Multispectral3D(0.90, 0.76, 0.46),	// ambient
		Multispectral3D(0.780392, 0.568627, 0.113725),	// diffuse
		Multispectral3D(0.992157, 0.941176, 0.807843),	// specular
		10.8,											// shininess
		1.54,											// refraciton index
		1.0,											// reflective %
		0.0);											// refractive %


	Material *glass3 = new Material(
		Multispectral3D(0.65, 0.77, 0.97),	// ambient
		Multispectral3D(0.65, 0.77, 0.97),	// diffuse
		Multispectral3D(0.65, 0.77, 0.97),	// specular
		10.8,											// shininess
		1.54,											// refraciton index
		1.0,											// reflective %
		0.0);											// refractive %

	Material *glass4 = new Material(
		Multispectral3D(0.90, 0.90, 0.90),	// ambient
		Multispectral3D(0.90, 0.90, 0.90),	// diffuse
		Multispectral3D(0.90, 0.90, 0.90),	// specular
		5.8,											// shininess
		1.54,											// refraciton index
		1.0,											// reflective %
		0.0);											// refractive %

	Point3D p0 = Point3D(-30, -4, -60);
	Point3D p1 = Point3D(-30, -4, 0);
	Point3D p2 = Point3D(30, -4, -60);
	Point3D p3 = Point3D(30, -4, 0);

	Point3D p4 = Point3D(-30, 20, -60);
	Point3D p5 = Point3D(30, 20, -60);

	static_scene.push_back(new Primitive(
			new Sphere(Point3D(0, -10004, -20), 10000), turquoise));
	static_scene.push_back(new Primitive(
			new Sphere(Point3D(5, -1, -15),  2), gold)); // yellow
	static_scene.push_back(new Primitive(
			new Sphere(Point3D(-5.5, 0, -15), 3), glass4)); // gray
	static_scene.push_back(new Primitive(
			new Sphere(Point3D(5, 0, -25), 3), glass3)); // blue

}
