// [header]
// A very basic raytracer example.
// [/header]
// [compile]
// c++ -o raytracer -O3 -Wall raytracer.cpp
// [/compile]
// [ignore]
// Copyright (C) 2012  www.scratchapixel.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// [/ignore]
#include <stdlib.h>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <vector>
#include <iostream>
#include <cassert>
#include "Sphere.h"
#include "Vec3.h"
// Windows only
#include <algorithm>
#include <sstream>
#include <string.h>
#include <chrono>
#include <thread>
#include <mutex>

#include "MemoryDebugger.h"
#include "MemoryPool.h"

#if defined __linux__ || defined __APPLE__
// "Compiled for Linux
#else
// Windows doesn't define these values by default, Linux does
#define M_PI 3.141592653589793
#define INFINITY 1e8
#endif

typedef Vec3<float> Vec3f;

//[comment]
// This variable controls the maximum recursion depth
//[/comment]
#define MAX_RAY_DEPTH 5

float mix(const float& a, const float& b, const float& mix)
{
	return b * mix + a * (1 - mix);
}

//[comment]
// This is the main trace function. It takes a ray as argument (defined by its origin
// and direction). We test if this ray intersects any of the geometry in the scene.
// If the ray intersects an object, we compute the intersection point, the normal
// at the intersection point, and shade this point using this information.
// Shading depends on the surface property (is it transparent, reflective, diffuse).
// The function returns a color for the ray. If the ray intersects an object that
// is the color of the object at the intersection point, otherwise it returns
// the background color.
//[/comment]
Vec3f trace(
	const Vec3f& rayorig,
	const Vec3f& raydir,
	const std::vector<Sphere>& spheres,
	const int& depth)
{
	//if (raydir.length() != 1) std::cerr << "Error " << raydir << std::endl;
	float tnear = INFINITY;
	const Sphere* sphere = NULL;
	// find intersection of this ray with the sphere in the scene
	for (unsigned i = 0; i < spheres.size(); ++i) {
		float t0 = INFINITY, t1 = INFINITY;
		if (spheres[i].intersect(rayorig, raydir, t0, t1)) {
			if (t0 < 0) t0 = t1;
			if (t0 < tnear) {
				tnear = t0;
				sphere = &spheres[i];
			}
		}
	}
	// if there's no intersection return black or background color
	if (!sphere) return Vec3f(2);
	Vec3f surfaceColor = 0; // color of the ray/surfaceof the object intersected by the ray
	Vec3f phit = rayorig + raydir * tnear; // point of intersection
	Vec3f nhit = phit - sphere->center; // normal at the intersection point
	nhit.normalize(); // normalize normal direction
	// If the normal and the view direction are not opposite to each other
	// reverse the normal direction. That also means we are inside the sphere so set
	// the inside bool to true. Finally reverse the sign of IdotN which we want
	// positive.
	float bias = 1e-4; // add some bias to the point from which we will be tracing
	bool inside = false;
	if (raydir.dot(nhit) > 0) nhit = -nhit, inside = true;
	if ((sphere->transparency > 0 || sphere->reflection > 0) && depth < MAX_RAY_DEPTH) {
		float facingratio = -raydir.dot(nhit);
		// change the mix value to tweak the effect
		float fresneleffect = mix(pow(1 - facingratio, 3), 1, 0.1);
		// compute reflection direction (not need to normalize because all vectors
		// are already normalized)

		Vec3f reflection = 0;
		Vec3f refraction = 0;

		//optimization: add reflection values only if reflection is present. Add transparency values only if its present.

		//if reflective, find reflection
		if (sphere->reflection) {
			Vec3f refldir = raydir - nhit * 2 * raydir.dot(nhit);
			refldir.normalize();
			Vec3f reflection = trace(phit + nhit * bias, refldir, spheres, depth + 1);
			surfaceColor += reflection * fresneleffect;
		}


		// if the sphere is also transparent compute refraction ray (transmission)
		if (sphere->transparency) {
			float ior = 1.1, eta = (inside) ? ior : 1 / ior; // are we inside or outside the surface?
			float cosi = -nhit.dot(raydir);
			float k = 1 - eta * eta * (1 - cosi * cosi);
			Vec3f refrdir = raydir * eta + nhit * (eta * cosi - sqrt(k));
			refrdir.normalize();
			refraction = trace(phit - nhit * bias, refrdir, spheres, depth + 1);
			surfaceColor += refraction * (1 - fresneleffect) * sphere->transparency;
		}

		// the result is a mix of reflection and refraction (if the sphere is transparent)
		surfaceColor *= sphere->surfaceColor;
	}
	else {
		// it's a diffuse object, no need to raytrace any further
		for (unsigned i = 0; i < spheres.size(); ++i) {
			if (spheres[i].emissionColor.x > 0) {
				// this is a light
				Vec3f transmission = 1;
				Vec3f lightDirection = spheres[i].center - phit;
				lightDirection.normalize();
				for (unsigned j = 0; j < spheres.size(); ++j) {
					if (i != j) {
						float t0, t1;
						if (spheres[j].intersect(phit + nhit * bias, lightDirection, t0, t1)) {
							transmission = 0;
							break;
						}
					}
				}
				surfaceColor += sphere->surfaceColor * transmission *
					std::max(float(0), nhit.dot(lightDirection)) * spheres[i].emissionColor;
			}
		}
	}

	return surfaceColor + sphere->emissionColor;
}
Vec3f traceThreadless(
	const Vec3f& rayorig,
	const Vec3f& raydir,
	const std::vector<Sphere*>& spheres,
	const int& depth,
	Vec3f& output)
{
	output = Vec3f(0);
	//if (raydir.length() != 1) std::cerr << "Error " << raydir << std::endl;
	float tnear = INFINITY;
	const Sphere* sphere = NULL;
	// find intersection of this ray with the sphere in the scene

	for (unsigned i = 0; i < spheres.size(); ++i) {
		float t0 = INFINITY, t1 = INFINITY;
		if (spheres[i]->intersect(rayorig, raydir, t0, t1)) {
			if (t0 < 0) t0 = t1;
			if (t0 < tnear) {
				tnear = t0;
				sphere = spheres[i];
			}
		}
	}
	// if there's no intersection return black or background color
	if (!sphere) {
		output = Vec3f(2);
		return output;
	}

	Vec3f surfaceColor = 0; // color of the ray/surfaceof the object intersected by the ray
	Vec3f phit = rayorig + raydir * tnear; // point of intersection
	Vec3f nhit = phit - sphere->center; // normal at the intersection point
	nhit.normalize(); // normalize normal direction
	// If the normal and the view direction are not opposite to each other
	// reverse the normal direction. That also means we are inside the sphere so set
	// the inside bool to true. Finally reverse the sign of IdotN which we want
	// positive.
	float bias = 1e-4; // add some bias to the point from which we will be tracing
	bool inside = false;
	if (raydir.dot(nhit) > 0) nhit = -nhit, inside = true;
	if ((sphere->transparency > 0 || sphere->reflection > 0) && depth < MAX_RAY_DEPTH) {
		float facingratio = -raydir.dot(nhit);
		// change the mix value to tweak the effect
		float fresneleffect = mix(pow(1 - facingratio, 3), 1, 0.1);
		// compute reflection direction (not need to normalize because all vectors
		// are already normalized)

		Vec3f reflection = 0;
		Vec3f refraction = 0;

		//optimization: add reflection values only if reflection is present. Add transparency values only if its present.

		//if reflective, find reflection
		if (sphere->reflection > 0) {
			Vec3f refldir = raydir - nhit * 2 * raydir.dot(nhit);
			refldir.normalize();
			traceThreadless(phit + nhit * bias, refldir, spheres, depth + 1, reflection);
			surfaceColor += reflection * fresneleffect;
		}


		// if the sphere is also transparent compute refraction ray (transmission)
		if (sphere->transparency > 0) {
			float ior = 1.1, eta = (inside) ? ior : 1 / ior; // are we inside or outside the surface?
			float cosi = -nhit.dot(raydir);
			float k = 1 - eta * eta * (1 - cosi * cosi);
			Vec3f refrdir = raydir * eta + nhit * (eta * cosi - sqrt(k));
			refrdir.normalize();
			traceThreadless(phit - nhit * bias, refrdir, spheres, depth + 1, refraction);
			surfaceColor += refraction * (1 - fresneleffect) * sphere->transparency;
		}

		// the result is a mix of reflection and refraction (if the sphere is transparent)
		surfaceColor *= sphere->surfaceColor;
	}
	else {
		// it's a diffuse object, no need to raytrace any further
		for (unsigned i = 0; i < spheres.size(); ++i) {
			Sphere temp = *spheres[i];
			if (temp.emissionColor.x > 0) {
				// this is a light
				Vec3f transmission = 1;
				Vec3f lightDirection = temp.center - phit;
				lightDirection.normalize();
				for (unsigned j = 0; j < spheres.size(); ++j) {
					if (i != j) {
						float t0, t1;
						if (spheres[j]->intersect(phit + nhit * bias, lightDirection, t0, t1)) {
							transmission = 0;
							break;
						}
					}
				}
				surfaceColor += sphere->surfaceColor * transmission *
					std::max(float(0), nhit.dot(lightDirection)) * temp.emissionColor;
			}
		}
	}
	output = surfaceColor + sphere->emissionColor;
	return output;
}
/////////////////////////////////////////////////////// my edit
Vec3f trace(
	const Vec3f& rayorig,
	const Vec3f& raydir,
	const std::vector<Sphere*>& spheres,
	const int& depth)
{
	//if (raydir.length() != 1) std::cerr << "Error " << raydir << std::endl;
	float tnear = INFINITY;
	const Sphere* sphere = NULL;
	// find intersection of this ray with the sphere in the scene

	for (unsigned i = 0; i < spheres.size(); ++i) {
		float t0 = INFINITY, t1 = INFINITY;
		if (spheres[i]->intersect(rayorig, raydir, t0, t1)) {
			if (t0 < 0) t0 = t1;
			if (t0 < tnear) {
				tnear = t0;
				sphere = spheres[i];
			}
		}
	}
	// if there's no intersection return black or background color
	if (!sphere) return Vec3f(2);
	Vec3f surfaceColor = 0; // color of the ray/surfaceof the object intersected by the ray
	Vec3f phit = rayorig + raydir * tnear; // point of intersection
	Vec3f nhit = phit - sphere->center; // normal at the intersection point
	nhit.normalize(); // normalize normal direction
	// If the normal and the view direction are not opposite to each other
	// reverse the normal direction. That also means we are inside the sphere so set
	// the inside bool to true. Finally reverse the sign of IdotN which we want
	// positive.
	float bias = 1e-4; // add some bias to the point from which we will be tracing
	bool inside = false;
	if (raydir.dot(nhit) > 0) nhit = -nhit, inside = true;
	if ((sphere->transparency > 0 || sphere->reflection > 0) && depth < MAX_RAY_DEPTH) {
		float facingratio = -raydir.dot(nhit);
		// change the mix value to tweak the effect
		float fresneleffect = mix(pow(1 - facingratio, 3), 1, 0.1);
		// compute reflection direction (not need to normalize because all vectors
		// are already normalized)

		Vec3f reflection = 0;
		Vec3f refraction = 0;


		std::thread reflectionThread;
		std::thread refractionThread;

		//optimization: add reflection values only if reflection is present. Add transparency values only if its present.

		//if reflective, find reflection
		if (sphere->reflection > 0) {
			Vec3f refldir = raydir - nhit * 2 * raydir.dot(nhit);
			refldir.normalize();
			//traceThreadless(phit + nhit * bias, refldir, spheres, depth + 1, reflection);
			reflectionThread = std::thread(traceThreadless, phit + nhit * bias, refldir, spheres, depth + 1, reflection);
		}


		// if the sphere is also transparent compute refraction ray (transmission)
		if (sphere->transparency > 0) {
			float ior = 1.1, eta = (inside) ? ior : 1 / ior; // are we inside or outside the surface?
			float cosi = -nhit.dot(raydir);
			float k = 1 - eta * eta * (1 - cosi * cosi);
			Vec3f refrdir = raydir * eta + nhit * (eta * cosi - sqrt(k));
			refrdir.normalize();
			//traceThreadless(phit - nhit * bias, refrdir, spheres, depth + 1, reflection);
			//refractionThread = std::thread(traceThreadless, phit - nhit * bias, refrdir, spheres, depth + 1, sphere, refraction);
			
		}

		if (reflectionThread.joinable()) 
		{
			reflectionThread.join();
			surfaceColor += reflection * fresneleffect;
		}
		//if (refractionThread.joinable()) 
		//{
		//	refractionThread.join();
		//	surfaceColor += refraction * (1 - fresneleffect) * sphere->transparency;
		//}

		// the result is a mix of reflection and refraction (if the sphere is transparent)
		surfaceColor *= sphere->surfaceColor;
	}
	else {
		// it's a diffuse object, no need to raytrace any further
		for (unsigned i = 0; i < spheres.size(); ++i) {
			Sphere temp = *spheres[i];
			if (temp.emissionColor.x > 0) {
				// this is a light
				Vec3f transmission = 1;
				Vec3f lightDirection = temp.center - phit;
				lightDirection.normalize();
				for (unsigned j = 0; j < spheres.size(); ++j) {
					if (i != j) {
						float t0, t1;
						if (spheres[j]->intersect(phit + nhit * bias, lightDirection, t0, t1)) {
							transmission = 0;
							break;
						}
					}
				}
				surfaceColor += sphere->surfaceColor * transmission *
					std::max(float(0), nhit.dot(lightDirection)) * temp.emissionColor;
			}
		}
	}

	return surfaceColor + sphere->emissionColor;
}



//[comment]
// Main rendering function. We compute a camera ray for each pixel of the image
// trace it and return a color. If the ray hits a sphere, we return the color of the
// sphere at the intersection point, else we return the background color.
//[/comment]
void render(const std::vector<Sphere>& spheres, int iteration)
{


	// Recommended Testing Resolution
	//unsigned const width = 640, height = 480;

	// Recommended Production Resolution
	unsigned width = 1920, height = 1080;
	Vec3f* image = new Vec3f[width * height], * pixel = image; //array of colors
	float invWidth = 2 / float(width), invHeight = 2 / float(height); //optimization: rather than multiplying by 2 on every iteration, just do it here once.
	float fov = 30, aspectratio = width / float(height);
	float angle = tan(M_PI * 0.5 * fov / 180.);
	float angleAndAspect = angle * aspectratio;


	// Trace rays
	Vec3f zero = Vec3f(0);
	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x, ++pixel) {
			//optimization: removing "+0.5" from xx and yy didnt make a difference to the output image
			float xx = (x * invWidth - 1) * angleAndAspect;
			float yy = (1 - y * invHeight) * angle;
			Vec3f raydir(xx, yy, -1);
			raydir.normalize();
			*pixel = trace(zero, raydir, spheres, 0);
		}
	}


	// Save result to a PPM image (keep these flags if you compile under Windows)
	std::stringstream ss;
	ss << "./spheres" << iteration << ".ppm";
	std::string tempString = ss.str();
	char* filename = (char*)tempString.c_str();

	std::ofstream ofs(filename, std::ios::out | std::ios::binary);
	ofs << "P6\n" << width << " " << height << "\n255\n";
	for (unsigned i = 0; i < width * height; ++i) {
		ofs << (unsigned char)(std::min(float(1), image[i].x) * 255) <<
			(unsigned char)(std::min(float(1), image[i].y) * 255) <<
			(unsigned char)(std::min(float(1), image[i].z) * 255);
	}
	ofs.close();
	delete[] image;
}
////////////////////////////////////////////////////////////////////////// my edit
void threadedRender(const std::vector<Sphere*>& spheres, Vec3f* pImage, std::mutex* data, const int maxSubdivisions, const int thisSubdivision, unsigned const width, unsigned const height)
{

	Vec3f* pixel = pImage; //copy of pointer to be used for iteration
	float invWidth = 2 / float(width), invHeight = 2 / float(height); //optimization: rather than multiplying by 2 on every iteration, just do it here once.
	float fov = 30, aspectratio = width / float(height);
	float angle = tan(M_PI * 0.5 * fov / 180.);
	float angleAndAspect = angle * aspectratio;

	//find subdivision location
	double YFraction = (double)height / maxSubdivisions;
	int startIndex = YFraction * thisSubdivision;
	int endIndex = YFraction * (thisSubdivision + 1);

	//find the start position of the pointer
	pixel = (Vec3f*)((char*)pixel + (startIndex * width * sizeof(Vec3f)));


	// Trace rays
	Vec3f zero = Vec3f(0);
	for (unsigned y = startIndex; y < endIndex; ++y) {
		for (unsigned x = 0; x < width; ++x, ++pixel) {
			//optimization: removing "+0.5" from xx and yy didnt make a difference to the output image
			float xx = (x * invWidth - 1) * angleAndAspect;
			float yy = (1 - y * invHeight) * angle;
			Vec3f raydir(xx, yy, -1);
 			raydir.normalize();
			//Vec3f temp = trace(zero, raydir, spheres, 0);
			Vec3f temp;
			traceThreadless(zero, raydir, spheres, 0, temp);
			//the threads don't fight over this resource, the mutex is not actually needed!
			//(*data).lock();
			*pixel = temp;
			//(*data).unlock();
		}
	}

}
void FileCreation(unsigned const width, unsigned const height, Vec3f* image, int iteration)
{
	// Save result to a PPM image (keep these flags if you compile under Windows)
	std::stringstream ss;
	ss << "./spheres" << iteration << ".ppm";
	std::string tempString = ss.str();
	char* filename = (char*)tempString.c_str();

	std::ofstream ofs(filename, std::ios::out | std::ios::binary);
	ofs << "P6\n" << width << " " << height << "\n255\n";
	for (unsigned i = 0; i < width * height; ++i) {
		ofs << (unsigned char)(std::min(float(1), image[i].x) * 255) <<
			(unsigned char)(std::min(float(1), image[i].y) * 255) <<
			(unsigned char)(std::min(float(1), image[i].z) * 255);
	}
	ofs.close();
}

void BasicRender()
{
	std::vector<Sphere> spheres;
	// Vector structure for Sphere (position, radius, surface color, reflectivity, transparency, emission color)

	spheres.push_back(Sphere(Vec3f(0.0, -10004, -20), 10000, Vec3f(0.20, 0.20, 0.20), 0, 0.0));
	spheres.push_back(Sphere(Vec3f(0.0, 0, -20), 4, Vec3f(1.00, 0.32, 0.36), 1, 0.5)); // The radius paramter is the value we will change
	spheres.push_back(Sphere(Vec3f(5.0, -1, -15), 2, Vec3f(0.90, 0.76, 0.46), 1, 0.0));
	spheres.push_back(Sphere(Vec3f(5.0, 0, -25), 3, Vec3f(0.65, 0.77, 0.97), 1, 0.0));

	auto start = std::chrono::system_clock::now();

	// This creates a file, titled 1.ppm in the current working directory
	render(spheres, 1);
	auto finish = std::chrono::system_clock::now();
	double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();

	std::cout << "Rendered and saved spheres" << 1 << ".ppm" << ". It took " << elapsedSeconds << "s to render and save." << std::endl;

	spheres.clear();

}

void SimpleShrinking()
{
	std::vector<Sphere> spheres;
	// Vector structure for Sphere (position, radius, surface color, reflectivity, transparency, emission color)

	for (int i = 0; i < 4; i++)
	{
		if (i == 0)
		{
			spheres.push_back(Sphere(Vec3f(0.0, -10004, -20), 10000, Vec3f(0.20, 0.20, 0.20), 0, 0.0));
			spheres.push_back(Sphere(Vec3f(0.0, 0, -20), 4, Vec3f(1.00, 0.32, 0.36), 1, 0.5)); // The radius paramter is the value we will change
			spheres.push_back(Sphere(Vec3f(5.0, -1, -15), 2, Vec3f(0.90, 0.76, 0.46), 1, 0.0));
			spheres.push_back(Sphere(Vec3f(5.0, 0, -25), 3, Vec3f(0.65, 0.77, 0.97), 1, 0.0));

		}
		else if (i == 1)
		{
			spheres.push_back(Sphere(Vec3f(0.0, -10004, -20), 10000, Vec3f(0.20, 0.20, 0.20), 0, 0.0));
			spheres.push_back(Sphere(Vec3f(0.0, 0, -20), 3, Vec3f(1.00, 0.32, 0.36), 1, 0.5)); // Radius--
			spheres.push_back(Sphere(Vec3f(5.0, -1, -15), 2, Vec3f(0.90, 0.76, 0.46), 1, 0.0));
			spheres.push_back(Sphere(Vec3f(5.0, 0, -25), 3, Vec3f(0.65, 0.77, 0.97), 1, 0.0));
		}
		else if (i == 2)
		{
			spheres.push_back(Sphere(Vec3f(0.0, -10004, -20), 10000, Vec3f(0.20, 0.20, 0.20), 0, 0.0));
			spheres.push_back(Sphere(Vec3f(0.0, 0, -20), 2, Vec3f(1.00, 0.32, 0.36), 1, 0.5)); // Radius--
			spheres.push_back(Sphere(Vec3f(5.0, -1, -15), 2, Vec3f(0.90, 0.76, 0.46), 1, 0.0));
			spheres.push_back(Sphere(Vec3f(5.0, 0, -25), 3, Vec3f(0.65, 0.77, 0.97), 1, 0.0));
		}
		else if (i == 3)
		{
			spheres.push_back(Sphere(Vec3f(0.0, -10004, -20), 10000, Vec3f(0.20, 0.20, 0.20), 0, 0.0));
			spheres.push_back(Sphere(Vec3f(0.0, 0, -20), 1, Vec3f(1.00, 0.32, 0.36), 1, 0.5)); // Radius--
			spheres.push_back(Sphere(Vec3f(5.0, -1, -15), 2, Vec3f(0.90, 0.76, 0.46), 1, 0.0));
			spheres.push_back(Sphere(Vec3f(5.0, 0, -25), 3, Vec3f(0.65, 0.77, 0.97), 1, 0.0));
		}

		render(spheres, i);
		// Dont forget to clear the Vector holding the spheres.
		spheres.clear();
	}
}

void SmoothScaling()
{
	//pool of 4 spheres initialized - allocates memory
	MemoryPool<Sphere>* spherePool = new MemoryPool<Sphere>(4);
	
	//construct 3 spheres in the pool
	// Vector structure for Sphere (position, radius, surface color, reflectivity, transparency, emission color)
	Sphere* sphere1 = new (spherePool) Sphere(Vec3f(0.0, -10004, -20), 10000, Vec3f(0.20, 0.20, 0.20), 0, 0.0);
	Sphere* sphere2 = new (spherePool) Sphere(Vec3f(5.0, -1, -15), 2, Vec3f(0.90, 0.76, 0.46), 1, 0.0);
	Sphere* sphere3 = new (spherePool) Sphere(Vec3f(5.0, 0, -25), 3, Vec3f(0.65, 0.77, 0.97), 1, 0.0);

	// Recommended Testing Resolution
	//unsigned const width = 640, height = 480;

	// Recommended Production Resolution
	unsigned width = 1920, height = 1080;
	int concurrency = std::thread::hardware_concurrency();
	//the trace function invokes 2 more threads for each thread running here, therefore divide the concurrency value by 2
	if (concurrency > 3)
		concurrency /= 2;
	else
		concurrency = 1;
	std::vector<std::thread*> threadList;
	concurrency = 1;
	//create the array of pixels and a mutex for it.
	std::mutex data;
	Vec3f* image = new Vec3f[width * height];
	
	//initialize the thread list
	for (int i = 0; i < concurrency; i++) {
		std::thread* t = new std::thread();
		threadList.push_back(t);
	}

	for (float r = 0; r <= 100; r++)
	{
		auto start = std::chrono::steady_clock::now();

		//construct the dynamic sphere
		Sphere* sphere4 = new (spherePool) Sphere(Vec3f(0.0, 0, -20), r / 100, Vec3f(1.00, 0.32, 0.36), 1, 0.5);




		//create a couple threads based on concurrency value

		for (int i = 0; i < concurrency; i++) {
			*threadList[i] = std::thread(threadedRender, spherePool->objects, image, &data, concurrency, i, width, height);
		}
		for (int i = 0; i < concurrency; i++)
		{
			threadList[i]->join();
		}


		//create the file here
		FileCreation(width, height, image, r);

		auto finish = std::chrono::steady_clock::now();
		double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();

		std::cout << "Rendered and saved spheres" << r << ".ppm" << ". It took " << elapsedSeconds << "s to render and save." << std::endl;



		// Release the dynamic sphere
		spherePool->ReleaseLast();
	}


#ifdef _DEBUG

	std::cout << std::endl;
	HeapManager::GetHeapByIndex((int)HeapID::Graphics)->WalkTheHeap();
#endif // DEBUG

	delete [] image;
	//release all the spheres and delete the memory pool. this calls the destructor, releasing all the objects within it.
	delete spherePool;

}
void SmoothScalingOriginal()
{
	std::vector<Sphere> spheres;
	// Vector structure for Sphere (position, radius, surface color, reflectivity, transparency, emission color)
	for (float r = 0; r <= 100; r++)
	{
		auto start = std::chrono::steady_clock::now();
		spheres.push_back(Sphere(Vec3f(0.0, -10004, -20), 10000, Vec3f(0.20, 0.20, 0.20), 0, 0.0));
		spheres.push_back(Sphere(Vec3f(0.0, 0, -20), r / 100, Vec3f(1.00, 0.32, 0.36), 1, 0.5)); // Radius++ change here
		spheres.push_back(Sphere(Vec3f(5.0, -1, -15), 2, Vec3f(0.90, 0.76, 0.46), 1, 0.0));
		spheres.push_back(Sphere(Vec3f(5.0, 0, -25), 3, Vec3f(0.65, 0.77, 0.97), 1, 0.0));
		render(spheres, r);
		auto finish = std::chrono::steady_clock::now();
		double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
		std::cout << "Rendered and saved spheres" << r << ".ppm" << ". It took " << elapsedSeconds << "s to render and save." << std::endl;
		// Dont forget to clear the Vector holding the spheres.
		spheres.clear();
	}
}
//[comment]
// In the main function, we will create the scene which is composed of 5 spheres
// and 1 light (which is also a sphere). Then, once the scene description is complete
// we render that scene, by calling the render() function.
//[/comment]
int main(int argc, char** argv)
{
	auto start = std::chrono::steady_clock::now();


	// This sample only allows one choice per program execution. Feel free to improve upon this
	srand(13);
	//BasicRender();
	//SimpleShrinking();
	SmoothScaling();
	//SmoothScalingOriginal();

	auto finish = std::chrono::steady_clock::now();
	double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
	std::cout << std::endl << "The entire process took " << elapsedSeconds << "s" << std::endl;

#ifdef  _DEBUG
	HeapManager::CleanUp();
#endif //  _DEBUG


	return 0;
}

