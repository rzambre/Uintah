
/*
 *  SquareWidget.h
 *
 *  Written by:
 *   James Purciful
 *   Department of Computer Science
 *   University of Utah
 *   Jan. 1994
 *
 *  Copyright (C) 1994 SCI Group
 */


#include <Widgets/SquareWidget.h>
#include <Constraints/DistanceConstraint.h>
#include <Constraints/HypotenuseConstraint.h>
#include <Constraints/PlaneConstraint.h>
#include <Geom/Cylinder.h>
#include <Geom/Sphere.h>

const Index NumCons = 8;
const Index NumVars = 6;
const Index NumGeoms = 8;
const Index NumMatls = 3;
const Index NumPcks = 5;
const Index NumSchemes = 2;

enum { SquareW_ConstULDR, SquareW_ConstURDL, SquareW_ConstHypo, SquareW_ConstPlane,
       SquareW_ConstULUR, SquareW_ConstULDL, SquareW_ConstDRUR, SquareW_ConstDRDL };
enum { SquareW_SphereUL, SquareW_SphereUR, SquareW_SphereDR, SquareW_SphereDL,
       SquareW_CylU, SquareW_CylR, SquareW_CylD, SquareW_CylL };
enum { SquareW_PointMatl, SquareW_EdgeMatl, SquareW_HighMatl };
enum { SquareW_PickSphUL, SquareW_PickSphUR, SquareW_PickSphDR, SquareW_PickSphDL, SquareW_PickCyls };

SquareWidget::SquareWidget( Module* module, Real widget_scale )
: BaseWidget(module, NumVars, NumCons, NumGeoms, NumMatls, NumPcks, widget_scale*0.1)
{
   cerr << "Starting SquareWidget CTOR" << endl;
   Real INIT = 1.0*widget_scale;
   variables[SquareW_PointUL] = new Variable("PntUL", Scheme1, Point(0, 0, 0));
   variables[SquareW_PointUR] = new Variable("PntUR", Scheme2, Point(INIT, 0, 0));
   variables[SquareW_PointDR] = new Variable("PntDR", Scheme1, Point(INIT, INIT, 0));
   variables[SquareW_PointDL] = new Variable("PntDL", Scheme2, Point(0, INIT, 0));
   variables[SquareW_Dist] = new Variable("DIST", Scheme1, Point(INIT, 0, 0));
   variables[SquareW_Hypo] = new Variable("HYPO", Scheme1, Point(sqrt(2*INIT*INIT), 0, 0));

   constraints[SquareW_ConstPlane] = new PlaneConstraint("ConstPlane",
							 NumSchemes,
							 variables[SquareW_PointUL],
							 variables[SquareW_PointUR],
							 variables[SquareW_PointDR],
							 variables[SquareW_PointDL]);
   
   constraints[SquareW_ConstPlane]->VarChoices(Scheme1, 2, 3, 0, 1);
   constraints[SquareW_ConstPlane]->VarChoices(Scheme2, 2, 3, 0, 1);
   constraints[SquareW_ConstPlane]->Priorities(P_Highest, P_Highest,
					      P_Highest, P_Highest);
   constraints[SquareW_ConstULDR] = new DistanceConstraint("Const13",
							  NumSchemes,
							  variables[SquareW_PointUL],
							  variables[SquareW_PointDR],
							  variables[SquareW_Hypo]);
   constraints[SquareW_ConstULDR]->VarChoices(Scheme1, 2, 2, 1);
   constraints[SquareW_ConstULDR]->VarChoices(Scheme2, 1, 0, 1);
   constraints[SquareW_ConstULDR]->Priorities(P_HighMedium, P_HighMedium, P_Default);
   constraints[SquareW_ConstURDL] = new DistanceConstraint("Const24",
							  NumSchemes,
							  variables[SquareW_PointUR],
							  variables[SquareW_PointDL],
							  variables[SquareW_Hypo]);
   constraints[SquareW_ConstURDL]->VarChoices(Scheme1, 1, 0, 1);
   constraints[SquareW_ConstURDL]->VarChoices(Scheme2, 2, 2, 1);
   constraints[SquareW_ConstURDL]->Priorities(P_HighMedium, P_HighMedium, P_Default);
   constraints[SquareW_ConstHypo] = new HypotenuseConstraint("ConstHypo",
							     NumSchemes,
							     variables[SquareW_Dist],
							     variables[SquareW_Hypo]);
   constraints[SquareW_ConstHypo]->VarChoices(Scheme1, 1, 0);
   constraints[SquareW_ConstHypo]->VarChoices(Scheme2, 1, 0);
   constraints[SquareW_ConstHypo]->Priorities(P_Default, P_HighMedium);
   constraints[SquareW_ConstULUR] = new DistanceConstraint("Const12",
							  NumSchemes,
							  variables[SquareW_PointUL],
							  variables[SquareW_PointUR],
							  variables[SquareW_Dist]);
   constraints[SquareW_ConstULUR]->VarChoices(Scheme1, 1, 1, 1);
   constraints[SquareW_ConstULUR]->VarChoices(Scheme2, 0, 0, 0);
   constraints[SquareW_ConstULUR]->Priorities(P_Default, P_Default, P_LowMedium);
   constraints[SquareW_ConstULDL] = new DistanceConstraint("Const14",
							  NumSchemes,
							  variables[SquareW_PointUL],
							  variables[SquareW_PointDL],
							  variables[SquareW_Dist]);
   constraints[SquareW_ConstULDL]->VarChoices(Scheme1, 1, 1, 1);
   constraints[SquareW_ConstULDL]->VarChoices(Scheme2, 0, 0, 0);
   constraints[SquareW_ConstULDL]->Priorities(P_Default, P_Default, P_LowMedium);
   constraints[SquareW_ConstDRUR] = new DistanceConstraint("Const32",
							  NumSchemes,
							  variables[SquareW_PointDR],
							  variables[SquareW_PointUR],
							  variables[SquareW_Dist]);
   constraints[SquareW_ConstDRUR]->VarChoices(Scheme1, 1, 1, 1);
   constraints[SquareW_ConstDRUR]->VarChoices(Scheme2, 0, 0, 0);
   constraints[SquareW_ConstDRUR]->Priorities(P_Default, P_Default, P_LowMedium);
   constraints[SquareW_ConstDRDL] = new DistanceConstraint("Const34",
							  NumSchemes,
							  variables[SquareW_PointDR],
							  variables[SquareW_PointDL],
							  variables[SquareW_Dist]);
   constraints[SquareW_ConstDRDL]->VarChoices(Scheme1, 1, 1, 1);
   constraints[SquareW_ConstDRDL]->VarChoices(Scheme2, 0, 0, 0);
   constraints[SquareW_ConstDRDL]->Priorities(P_Default, P_Default, P_LowMedium);

   materials[SquareW_PointMatl] = new Material(Color(0,0,0), Color(.54, .60, 1),
					      Color(.5,.5,.5), 20);
   materials[SquareW_EdgeMatl] = new Material(Color(0,0,0), Color(.54, .60, .66),
					     Color(.5,.5,.5), 20);
   materials[SquareW_HighMatl] = new Material(Color(0,0,0), Color(.7,.7,.7),
					     Color(0,0,.6), 20);

   Index geom, pick;
   GeomGroup* pts = new GeomGroup;
   for (geom = SquareW_SphereUL, pick = SquareW_PickSphUL;
	geom <= SquareW_SphereDL; geom++, pick++) {
      geometries[geom] = new GeomSphere;
      picks[pick] = new GeomPick(geometries[geom], module);
      picks[pick]->set_highlight(materials[SquareW_HighMatl]);
      picks[pick]->set_cbdata((void*)pick);
      pts->add(picks[pick]);
   }
   GeomMaterial* ptsm = new GeomMaterial(pts, materials[SquareW_PointMatl]);
   
   GeomGroup* cyls = new GeomGroup;
   for (geom = SquareW_CylU; geom <= SquareW_CylL; geom++) {
      geometries[geom] = new GeomCylinder;
      cyls->add(geometries[geom]);
   }
   picks[SquareW_PickCyls] = new GeomPick(cyls, module);
   picks[SquareW_PickCyls]->set_highlight(materials[SquareW_HighMatl]);
   picks[SquareW_PickCyls]->set_cbdata((void*)SquareW_PickCyls);
   GeomMaterial* cylsm = new GeomMaterial(picks[SquareW_PickCyls], materials[SquareW_EdgeMatl]);

   GeomGroup* w = new GeomGroup;
   w->add(ptsm);
   w->add(cylsm);

   FinishWidget(w);
   
   SetEpsilon(widget_scale*1e-4);
   
   // Init variables.
   for (Index vindex=0; vindex<NumVariables; vindex++)
      variables[vindex]->Order();
   
   for (vindex=0; vindex<NumVariables; vindex++)
      variables[vindex]->Resolve();
   cerr << "Done with SquareWidget CTOR" << endl;
}


SquareWidget::~SquareWidget()
{
}


void
SquareWidget::execute()
{
   cerr << "Execute called..." << endl;
   cerr << "widget_scale=" << widget_scale << endl;
   
   ((GeomSphere*)geometries[SquareW_SphereUL])->move(variables[SquareW_PointUL]->Get(),
						    1*widget_scale);
   ((GeomSphere*)geometries[SquareW_SphereUR])->move(variables[SquareW_PointUR]->Get(),
						    1*widget_scale);
   ((GeomSphere*)geometries[SquareW_SphereDR])->move(variables[SquareW_PointDR]->Get(),
						    1*widget_scale);
   ((GeomSphere*)geometries[SquareW_SphereDL])->move(variables[SquareW_PointDL]->Get(),
						    1*widget_scale);
   ((GeomCylinder*)geometries[SquareW_CylU])->move(variables[SquareW_PointUL]->Get(),
						  variables[SquareW_PointUR]->Get(),
						  0.5*widget_scale);
   ((GeomCylinder*)geometries[SquareW_CylR])->move(variables[SquareW_PointUR]->Get(),
						  variables[SquareW_PointDR]->Get(),
						  0.5*widget_scale);
   ((GeomCylinder*)geometries[SquareW_CylD])->move(variables[SquareW_PointDR]->Get(),
						  variables[SquareW_PointDL]->Get(),
						  0.5*widget_scale);
   ((GeomCylinder*)geometries[SquareW_CylL])->move(variables[SquareW_PointDL]->Get(),
						  variables[SquareW_PointUL]->Get(),
						  0.5*widget_scale);

   SetEpsilon(widget_scale*1e-4);

   Vector spvec1(variables[SquareW_PointUR]->Get() - variables[SquareW_PointUL]->Get());
   Vector spvec2(variables[SquareW_PointDL]->Get() - variables[SquareW_PointUL]->Get());
   spvec1.normalize();
   spvec2.normalize();
   Vector v = Cross(spvec1, spvec2);
   for (Index geom = 0; geom < NumPcks; geom++) {
      picks[geom]->set_principal(spvec1, spvec2, v);
   }
}

void
SquareWidget::geom_moved( int /* axis */, double /* dist */, const Vector& delta,
			 void* cbdata )
{
   Vector delt = delta;
   cerr << "Moved called..." << endl;
   switch((int)cbdata){
   case SquareW_PickSphUL:
      cerr << "  SquareW_SphereUL moved" << endl;
      variables[SquareW_PointUL]->SetDelta(delt);
      break;
   case SquareW_PickSphUR:
      cerr << "  SquareW_SphereUR moved" << endl;
      variables[SquareW_PointUR]->SetDelta(delt);
      break;
   case SquareW_PickSphDR:
      cerr << "  SquareW_SphereDR moved" << endl;
      variables[SquareW_PointDR]->SetDelta(delt);
      break;
   case SquareW_PickSphDL:
      cerr << "  SquareW_SphereDL moved" << endl;
      variables[SquareW_PointDL]->SetDelta(delt);
      break;
   case SquareW_PickCyls:
      cerr << "  SquareW_CylU moved" << endl;
      cerr << "  SquareW_CylR moved" << endl;
      cerr << "  SquareW_CylD moved" << endl;
      cerr << "  SquareW_CylL moved" << endl;
      variables[SquareW_PointUL]->MoveDelta(delt);
      variables[SquareW_PointUR]->MoveDelta(delt);
      variables[SquareW_PointDR]->MoveDelta(delt);
      variables[SquareW_PointDL]->MoveDelta(delt);
      break;
   }
}

