
/*
 *  View.h:  The camera
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   September 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifndef SCI_Geom_View_h
#define SCI_Geom_View_h 1

#include <Geometry/Point.h>
#include <Geometry/Vector.h>

struct View {
    Point eyep;
    Point lookat;
    Vector up;
    double fov;
    View();
    View(const Point&, const Point&, const Vector&, double);
    View(const View&);
    ~View();
    View& operator=(const View&);
};

#endif /* SCI_Geom_View_h */
