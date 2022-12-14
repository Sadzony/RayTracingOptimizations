#include <stdio.h>
#include <stdlib.h>
#include <scebase.h>
#include <kernel.h>
#include <gnmx.h>
#include <video_out.h>

#include <cstdio>
#include <cmath>
#include <fstream>
#include <vector>
#include <iostream>
#include <cassert>
#include <stdio.h>

#include <algorithm>
#include <sstream>
#include <string.h>



// Time precision
#include <chrono>

// Threading
#include <thread>
#include <libsysmodule.h>
#include <ult.h>

#include "allocator.h"

//memory
#include "MemoryDebugger.h"
#include "MemoryPool.h"



static const size_t kOnionMemorySize = 64 * 1024 * 1024;

std::chrono::time_point<std::chrono::system_clock> start;
std::chrono::time_point<std::chrono::system_clock> end;
std::chrono::duration<double> total_elapsed_time;

#define CONTEXT_SIZE 16384
#define NUM_THREADS 12
static uint64_t workerContextBuffer[NUM_THREADS][CONTEXT_SIZE / sizeof(uint64_t)];

using namespace sce;
using namespace sce::Gnmx;

template<typename T>
class Vec3
{
public:
	T x, y, z;
	Vec3() : x(T(0)), y(T(0)), z(T(0)) {}
	Vec3(T xx) : x(xx), y(xx), z(xx) {}
	Vec3(T xx, T yy, T zz) : x(xx), y(yy), z(zz) {}
	Vec3& normalize()
	{
		T nor2 = length2();
		if (nor2 > 0) {
			T invNor = 1 / sqrt(nor2);
			x *= invNor, y *= invNor, z *= invNor;
		}
		return *this;
	}
	Vec3<T> operator * (const T& f) const { return Vec3<T>(x * f, y * f, z * f); }
	Vec3<T> operator * (const Vec3<T>& v) const { return Vec3<T>(x * v.x, y * v.y, z * v.z); }
	T dot(const Vec3<T>& v) const { return x * v.x + y * v.y + z * v.z; }
	Vec3<T> operator - (const Vec3<T>& v) const { return Vec3<T>(x - v.x, y - v.y, z - v.z); }
	Vec3<T> operator + (const Vec3<T>& v) const { return Vec3<T>(x + v.x, y + v.y, z + v.z); }
	Vec3<T>& operator += (const Vec3<T>& v) { x += v.x, y += v.y, z += v.z; return *this; }
	Vec3<T>& operator *= (const Vec3<T>& v) { x *= v.x, y *= v.y, z *= v.z; return *this; }
	Vec3<T> operator - () const { return Vec3<T>(-x, -y, -z); }
	T length2() const { return x * x + y * y + z * z; }
	T length() const { return sqrt(length2()); }
	friend std::ostream& operator << (std::ostream& os, const Vec3<T>& v)
	{
		os << "[" << v.x << " " << v.y << " " << v.z << "]";
		return os;
	}
};

typedef Vec3<float> Vec3f;

class Sphere
{
public:
	Vec3f center;                           /// position of the sphere
	float radius, radius2;                  /// sphere radius and radius^2
	Vec3f surfaceColor, emissionColor;      /// surface color and emission (light)
	float transparency, reflection;         /// surface transparency and reflectivity
	Sphere(
		const Vec3f& c,
		const float& r,
		const Vec3f& sc,
		const float& refl = 0,
		const float& transp = 0,
		const Vec3f& ec = 0) :
		center(c), radius(r), radius2(r* r), surfaceColor(sc), emissionColor(ec),
		transparency(transp), reflection(refl)
	{ /* empty */
	}
	//[comment]
	// Compute a ray-sphere intersection using the geometric solution
	//[/comment]
	bool intersect(const Vec3f& rayorig, const Vec3f& raydir, float& t0, float& t1) const
	{
		Vec3f l = center - rayorig;
		float tca = l.dot(raydir);
		if (tca < 0) return false;
		float d2 = l.dot(l) - tca * tca;
		if (d2 > radius2) return false;
		float thc = sqrt(radius2 - d2);
		t0 = tca - thc;
		t1 = tca + thc;

		return true;
	}
};

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


		// if the sphere is also transparent compute refraction ray (transmission)
		if (sphere->reflection > 0) {
			Vec3f refldir = raydir - nhit * 2 * raydir.dot(nhit);
			refldir.normalize();
			reflection = trace(phit + nhit * bias, refldir, spheres, depth + 1);
			surfaceColor += reflection * fresneleffect;
		}
		if (sphere->transparency > 0) {
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
			if (spheres[i]->emissionColor.x > 0) {
				// this is a light
				Vec3f transmission = 1;
				Vec3f lightDirection = spheres[i]->center - phit;
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
					std::max(float(0), nhit.dot(lightDirection)) * spheres[i]->emissionColor;
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
void render(const std::vector<Sphere*>& spheres, int iteration, Vec3f* image, const int maxSubdivisions, const int thisSubdivision, unsigned const width, unsigned const height)
{
	Vec3f* pixel = image; //copy of pointer to be used for iteration
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
			Vec3f temp = trace(zero, raydir, spheres, 0);
			*pixel = temp;
		}
	}
}

void FileCreation(int width, int height, Vec3f* image, int iteration)
{
	// Save result to a PPM image (keep these flags if you compile under Windows)
	std::stringstream ss;
	ss << "/app0/spheres" << iteration << ".ppm";
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

struct attributes 
{
	std::vector<Sphere*>* spheres;
	int iteration;
	Vec3f* image;
	int maxSubdivisions;
	int thisSubdivision;
	int width;
	int height;
};

//wrapper for the render function
int32_t renderThreadEntry(uint64_t arg)
{
	attributes* attr = (attributes*)arg;
	render(*attr->spheres, attr->iteration, attr->image, attr->maxSubdivisions, attr->thisSubdivision, attr->width, attr->height);
	return SCE_OK;
}


void BasicRender(int iteration, std::vector<Sphere*>& spheres, int concurrencyVal, SceUltUlthreadRuntime& runtime)
{
	auto start = std::chrono::system_clock::now(); //start counting

	// Initialize the WB_ONION memory allocator

	LinearAllocator onionAllocator;
	int ret = onionAllocator.initialize(
		kOnionMemorySize, SCE_KERNEL_WB_ONION,
		SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_ALL);

	//if (ret != SCE_OK)
	//	return ret;

	unsigned width = 1920, height = 1080;
	size_t totalSize = sizeof(Vec3f) * width * height;

	void* buffer = onionAllocator.allocate(totalSize, Gnm::kAlignmentOfBufferInBytes);

	Vec3f* image = reinterpret_cast<Vec3f*>(buffer);





	uint64_t threadContextSize = CONTEXT_SIZE; //amount of allocated data per thread
	SceUltUlthread threads[NUM_THREADS];
	attributes args[NUM_THREADS] __attribute__((aligned(8)));
	//create user level threads
	for (int i = 0; i < NUM_THREADS; i++) {

		//setup arguments for the entry function
		args[i].spheres = &spheres;
		args[i].image = image;
		args[i].iteration = iteration;
		args[i].maxSubdivisions = NUM_THREADS;
		args[i].width = width;
		args[i].height = height;
		args[i].thisSubdivision = i;

		//create thread
		ret = sceUltUlthreadCreate(&threads[i], "renderThread", renderThreadEntry, (uint64_t)&args[i], workerContextBuffer[i], threadContextSize, &runtime, NULL);
		assert(ret == SCE_OK);
	}

	//join threads
	for (int i = 0; i < NUM_THREADS; i++) {
		int32_t status;
		ret = sceUltUlthreadJoin(&threads[i], &status);
		assert(ret == SCE_OK);
	}

	//Create the file
	FileCreation(width, height, image, iteration);


	auto finish = std::chrono::system_clock::now();

	double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
	std::cout << "Rendered and saved spheres" << iteration << ".ppm" << ". It took " << elapsedSeconds << "s to render and save." << std::endl;
}
int main(int argc, char** argv)
{

	//load the Ult library
	if (sceSysmoduleLoadModule(SCE_SYSMODULE_ULT) == SCE_OK) {
		sceUltInitialize();
		auto start = std::chrono::steady_clock::now();

		MemoryPool<Sphere>* spherePool = new MemoryPool <Sphere>(4);
		Sphere* sphere1 = new (spherePool) Sphere(Vec3f(0.0, -10004, -20), 10000, Vec3f(0.20, 0.20, 0.20), 0, 0.0);
		Sphere* sphere2 = new (spherePool) Sphere(Vec3f(5.0, -1, -15), 2, Vec3f(0.90, 0.76, 0.46), 1, 0.0);
		Sphere* sphere3 = new (spherePool) Sphere(Vec3f(5.0, 0, -25), 3, Vec3f(0.65, 0.77, 0.97), 1, 0.0);

		//int concurrency = std::thread::hardware_concurrency();
		int concurrency = NUM_THREADS;

		//initialize the runtime
		SceUltUlthreadRuntime runtime;
		uint64_t workAreasize = sceUltUlthreadRuntimeGetWorkAreaSize(concurrency, NUM_THREADS);
		void* runtimeBuffer = malloc(workAreasize);
		uint64_t ret = sceUltUlthreadRuntimeCreate(&runtime, "renderRuntime", concurrency, NUM_THREADS, runtimeBuffer, NULL);

		for (int i = 0; i < 10; i++)
		{
			Sphere* sphere4 = new (spherePool) Sphere(Vec3f(i, 0, -20), 1, Vec3f(1.00, 0.32, 0.36), 1, 0.5);
			BasicRender(i, spherePool->objects, concurrency, runtime);
			spherePool->ReleaseLast();
		}

		//destroy the runtime and free work area

		sceUltUlthreadRuntimeDestroy(&runtime);
		free(runtimeBuffer);

		auto finish = std::chrono::steady_clock::now();
		double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
		std::cout << std::endl << "The entire process took " << elapsedSeconds << "s" << std::endl;
		spherePool->ReleaseObjects();
		delete spherePool;
	}
	sceUltFinalize();
	sceSysmoduleUnloadModule(SCE_SYSMODULE_ULT);
	return 0;
}