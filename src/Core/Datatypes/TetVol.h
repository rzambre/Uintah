/*
 *  TetVol.h
 *
 *  Written by:
 *   Martin Cole
 *   School of Computing
 *   University of Utah
 *
 *  Copyright (C) 2001 SCI Institute
 */

#ifndef Datatypes_TetVol_h
#define Datatypes_TetVol_h

#include <Core/Datatypes/Field.h>
#include <Core/Datatypes/TetVolMesh.h>
#include <Core/Datatypes/GenericField.h>
#include <Core/Containers/LockingHandle.h>
#include <Core/Persistent/PersistentSTL.h>
#include <vector>


namespace SCIRun {

template <class T> 
class TetVol : public GenericField<TetVolMesh, vector<T> > {
public:
  TetVol() : 
    GenericField<TetVolMesh, vector<T> >() {};
  TetVol(Field::data_location data_at) : 
    GenericField<TetVolMesh, vector<T> >(data_at) {};
  virtual ~TetVol() {};

  void    io(Piostream &stream);
  static  PersistentTypeID type_id;
  //static const string type_name(int a);
  static const string type_name(int n = -1);
  virtual const string get_type_name(int n = -1) const { return type_name(n); }

private:
  static Persistent *maker();
};

// Pio defs.
const double TET_VOL_VERSION = 1.0;

template <class T>
Persistent*
TetVol<T>::maker()
{
  return scinew TetVol<T>;
}

template <class T>
PersistentTypeID 
TetVol<T>::type_id(type_name(), 
		   GenericField<TetVolMesh, vector<T> >::type_name(),
		   maker);


template <class T>
void 
TetVol<T>::io(Piostream& stream)
{
  stream.begin_class(type_name().c_str(), TET_VOL_VERSION);
  GenericField<TetVolMesh, vector<T> >::io(stream);
  stream.end_class();
}

template <class T> 
const string 
TetVol<T>::type_name(int a)
{
  ASSERT((a <= 1) && a >= -1);
  if (a == -1) {
    return "TetVol<" + find_type_name((T *)0) + ">";
  } else if (a == 0) { return "TetVol"; }
  return find_type_name((T *)0);
}

} // end namespace SCIRun

#endif // Datatypes_TetVol_h



















