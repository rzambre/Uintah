//  
//  For more information, please see: http://software.sci.utah.edu
//  
//  The MIT License
//  
//  Copyright (c) 2004 Scientific Computing and Imaging Institute,
//  University of Utah.
//  
//  License for the specific language governing rights and limitations under
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//  
//  The above copyright notice and this permission notice shall be included
//  in all copies or substantial portions of the Software.
//  
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//  
//    File   : TextureRenderer.h
//    Author : Milan Ikits
//    Date   : Wed Jul  7 23:34:33 2004

#ifndef TextureRenderer_h
#define TextureRenderer_h

#include <Core/Thread/Mutex.h>
#include <Core/Geometry/Point.h>
#include <Core/Geometry/Ray.h>
#include <Core/Geometry/Vector.h>
#include <Core/Geometry/Transform.h>
#include <Core/Geom/ColorMap.h>
#include <Core/Geom/GeomObj.h>

#include <Core/Containers/BinaryTree.h>
#include <Core/Containers/Array2.h>
#include <Core/Containers/Array3.h>

#include <Packages/Volume/Core/Datatypes/Brick.h>
#include <Packages/Volume/Core/Datatypes/Texture.h>
#include <Packages/Volume/Core/Datatypes/Colormap2.h>
#include <Packages/Volume/Core/Datatypes/CM2Widget.h>

namespace Volume {

using SCIRun::GeomObj;
using SCIRun::DrawInfoOpenGL;
using SCIRun::ColorMap;
using SCIRun::Mutex;

class Pbuffer;
class FragmentProgramARB;
class VolShaderFactory;

class TextureRenderer : public GeomObj
{
public:
  TextureRenderer(TextureHandle tex, ColorMapHandle cmap1, Colormap2Handle cmap2);
  TextureRenderer(const TextureRenderer&);
  virtual ~TextureRenderer();

  void set_texture(TextureHandle tex);
  void set_colormap1(ColorMapHandle cmap1);
  void set_colormap2(Colormap2Handle cmap2);
  void set_colormap_size(int size);
  void set_slice_alpha(double alpha);
  void set_sw_raster(bool b);
  bool use_pbuffer() { return use_pbuffer_; }
  void set_blend_numbits(int b);
  inline void set_interp(bool i) { interp_ = i; }
  
  enum RenderMode { MODE_NONE, MODE_OVER, MODE_MIP, MODE_SLICE };
    
#ifdef SCI_OPENGL
  virtual void draw(DrawInfoOpenGL*, Material*, double time) = 0;
  virtual void draw() = 0;
  virtual void draw_wireframe() = 0;
#endif
  
  virtual GeomObj* clone() = 0;
  virtual void get_bounds(BBox& bb){ tex_->get_bounds( bb ); }
  virtual void io(Piostream&);
  static PersistentTypeID type_id;
  virtual bool saveobj(std::ostream&, const string& format, GeomSave*);

  //TextureHandle texH() const { return tex_; }
  //DrawInfoOpenGL* di() const { return di_; }
  //bool interp() const { return interp_; }

protected:
  TextureHandle tex_;
  Mutex mutex_;
  ColorMapHandle cmap1_;
  Colormap2Handle cmap2_;
  bool cmap1_dirty_;
  bool cmap2_dirty_;
  bool alpha_dirty_;
  RenderMode mode_;
  bool interp_;
  int lighting_;
  double sampling_rate_;
  double irate_;
  bool imode_;
  double slice_alpha_;
  bool sw_raster_;
  DrawInfoOpenGL* di_;

#ifdef SCI_OPENGL  
  int cmap_size_;
  Array2<unsigned char> cmap1_array_;
  unsigned int cmap1_tex_;
  Array3<float> raster_array_;
  Array3<unsigned char> cmap2_array_;
  unsigned int cmap2_tex_;
  bool use_pbuffer_;
  Pbuffer* raster_buffer_;
  CM2ShaderFactory* shader_factory_;
  Pbuffer* cmap2_buffer_;
  FragmentProgramARB* cmap2_shader_nv_;
  FragmentProgramARB* cmap2_shader_ati_;
  VolShaderFactory* vol_shader_factory_;
  Pbuffer* blend_buffer_;
  int blend_numbits_;
  
  void compute_view(Ray& ray);
  void load_brick(Brick& b);
  void draw_polys(vector<Polygon *> polys, bool z);
  void build_colormap1();
  void build_colormap2();
  void bind_colormap1();
  void bind_colormap2();
  void release_colormap1();
  void release_colormap2();
#endif
};



} // end namespace Volume

#endif // TextureRenderer_h
