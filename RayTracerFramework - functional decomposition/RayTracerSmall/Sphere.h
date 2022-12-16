#pragma once
#include "Vec3.h"
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
		Vec3f l = center - rayorig; //line from ray origin to sphere centre
		float tca = l.dot(raydir); //dot product the line with ray direction
		if (tca < 0) return false; //if dot product is negative, they face away from each other so negative
		float d2 = l.dot(l) - tca * tca; //how far the direction vector is from the centre (squared)
		if (d2 > radius2) return false; //if its further than the radius, return false
		float thc = sqrt(radius2 - d2); //how far on the radius the hit is
		t0 = tca - thc; //distance to entry point
		t1 = tca + thc; //distance to exit point

		return true;
	}
};