/**************************************************************************\
 *
 *  This file is part of the Coin 3D visualization library.
 *  Copyright (C) 1998-2000 by Systems in Motion. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  version 2.1 as published by the Free Software Foundation. See the
 *  file LICENSE.LGPL at the root directory of the distribution for
 *  more details.
 *
 *  If you want to use Coin for applications not compatible with the
 *  LGPL, please contact SIM to acquire a Professional Edition license.
 *
 *  Systems in Motion, Prof Brochs gate 6, 7030 Trondheim, NORWAY
 *  http://www.sim.no support@sim.no Voice: +47 22114160 Fax: +47 22207097
 *
\**************************************************************************/

/*!
  \class SoAsciiText SoAsciiText.h Inventor/nodes/SoAsciiText.h
  \brief The SoAsciiText class renders flat 3D text.
  \ingroup nodes

  The text is rendered using 3D polygon geometry.

  This node is different from the SoText2 node in that it rotates,
  scales, translates etc just like other geometry in the scene. It is
  different from the SoText3 node in that it renders the text "flat",
  i.e. does not extrude the fonts to have depth.
*/

#include <Inventor/nodes/SoAsciiText.h>
#include <Inventor/nodes/SoSubNodeP.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/bundles/SoMaterialBundle.h>
#include <Inventor/details/SoTextDetail.h>
#include <Inventor/elements/SoFontNameElement.h>
#include <Inventor/elements/SoFontSizeElement.h>
#include <Inventor/elements/SoGLNormalizeElement.h>
#include <Inventor/elements/SoGLShapeHintsElement.h>
#include <Inventor/elements/SoGLTextureEnabledElement.h>
#include <Inventor/misc/SoGlyph.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/sensors/SoFieldSensor.h>

#ifdef HAVE_WINDOWS_H
#include <windows.h> // *sigh* needed for gl.h
#endif // HAVE_WINDOWS_H
#include <GL/gl.h>

/*!
  \enum SoAsciiText::Justification
  Font justification values.
*/


/*!
  \var SoMFString SoAsciiText::string
  Lines of text to render. Default value is empty.
*/
/*!
  \var SoSFFloat SoAsciiText::spacing
  Spacing between each line. Defaults to 1.0.
*/
/*!
  \var SoSFEnum SoAsciiText::justification
  Horizontal alignment. Default SoAsciiText::LEFT.
*/

/*!
  \var SoMFFloat SoAsciiText::width
  Defines the width of each line.
*/


// *************************************************************************

SO_NODE_SOURCE(SoAsciiText);

/*!
  Constructor.
*/
SoAsciiText::SoAsciiText(void)
{
  SO_NODE_INTERNAL_CONSTRUCTOR(SoAsciiText);

  SO_NODE_ADD_FIELD(string, (""));
  SO_NODE_ADD_FIELD(spacing, (1.0f));
  SO_NODE_ADD_FIELD(justification, (SoAsciiText::LEFT));
  SO_NODE_ADD_FIELD(width, (0.0f));

  SO_NODE_DEFINE_ENUM_VALUE(Justification, LEFT);
  SO_NODE_DEFINE_ENUM_VALUE(Justification, RIGHT);
  SO_NODE_DEFINE_ENUM_VALUE(Justification, CENTER);
  SO_NODE_SET_SF_ENUM_TYPE(justification, Justification);

  this->stringsensor = new SoFieldSensor(SoAsciiText::fieldSensorCB, this);
  this->stringsensor->attach(&this->string);

  this->needsetup = TRUE;
}

/*!
  Destructor.
*/
SoAsciiText::~SoAsciiText()
{
  delete this->stringsensor;
}

// Doc in parent.
void
SoAsciiText::initClass(void)
{
  SO_NODE_INTERNAL_INIT_CLASS(SoAsciiText);
}

// Doc in parent.
void
SoAsciiText::GLRender(SoGLRenderAction * action)
{
  if (!this->shouldGLRender(action)) return;

  SoState * state = action->getState();
  this->setUpGlyphs(state);

  SoMaterialBundle mb(action);
  mb.sendFirst();

  const SoGLShapeHintsElement * sh = (SoGLShapeHintsElement *)
    state->getConstElement(SoGLShapeHintsElement::getClassStackIndex());
  // turn on backface culling. Disable twoside lighting
  sh->forceSend(TRUE, TRUE, FALSE);

  const SoGLNormalizeElement * ne = (SoGLNormalizeElement *)
    state->getConstElement(SoGLNormalizeElement::getClassStackIndex());
  ne->forceSend(TRUE); // we will provide unit length normals

  float size = SoFontSizeElement::get(state);
  SbBool doTextures = SoGLTextureEnabledElement::get(state);

  int i, n = this->string.getNum();

  glBegin(GL_TRIANGLES);
  glNormal3f(0.0f, 0.0f, 1.0f);

  int glyphidx = 0;
  float ypos = 0.0f;

  for (i = 0; i < n; i++) {
    float currwidth = this->getWidth(i, size);
    float xpos = 0.0f;
    switch (this->justification.getValue()) {
    case SoAsciiText::RIGHT:
      xpos = -currwidth * size;
      break;
    case SoAsciiText::CENTER:
      xpos = -currwidth * size * 0.5f;
      break;
    }

    const char * str = this->string[i].getString();
    int len = strlen(str);
    float horizspacing = 0.0f;
    if (len > 1) {
      horizspacing = ((currwidth - this->glyphwidths[i]) / (len - 1)) * size;
    }

    while (*str++) {
      const SoGlyph * glyph = this->glyphs[glyphidx++];
      const SbVec2f * coords = glyph->getCoords();
      const int * ptr = glyph->getFaceIndices();
      while (*ptr >= 0) {
        SbVec2f v0, v1, v2;
        v0 = coords[*ptr++];
        v1 = coords[*ptr++];
        v2 = coords[*ptr++];
        if (doTextures) {
          glTexCoord2f(v0[0], v0[1]);
        }
        glVertex3f(v0[0] * size + xpos, v0[1] * size + ypos, 0.0f);
        if (doTextures) {
          glTexCoord2f(v1[0], v1[1]);
        }
        glVertex3f(v1[0] * size + xpos, v1[1] * size + ypos, 0.0f);
        if (doTextures) {
          glTexCoord2f(v2[0], v2[1]);
        }
        glVertex3f(v2[0] * size + xpos, v2[1] * size + ypos, 0.0f);
      }
      xpos += glyph->getWidth() * size + horizspacing;
    }
    ypos -= size * this->spacing.getValue();
  }
  glEnd();
}

// Doc in parent.
void
SoAsciiText::getPrimitiveCount(SoGetPrimitiveCountAction * action)
{
  if (action->is3DTextCountedAsTriangles()) {
    int n = this->glyphs.getLength();
    int numtris = 0;
    for (int i = 0; i < n; i++) {
      const SoGlyph * glyph = this->glyphs[i];
      int cnt = 0;
      const int * ptr = glyph->getFaceIndices();
      while (*ptr++ >= 0) cnt++;
      numtris += cnt / 3;
    }
    action->addNumTriangles(numtris);
  }
  else {
    action->addNumText(this->string.getNum());
  }
}

// Doc in parent.
void
SoAsciiText::computeBBox(SoAction * action, SbBox3f & box, SbVec3f & center)
{
  this->setUpGlyphs(action->getState());

  float size = SoFontSizeElement::get(action->getState());
  int i, n = this->string.getNum();
  if (n == 0 || this->glyphs.getLength() == 0) {
    center = SbVec3f(0.0f, 0.0f, 0.0f);
    box.setBounds(center, center);
    return;
  }

  float maxw = this->getWidth(0, size);
  for (i = 1; i < n; i++) {
    float tmp = this->getWidth(i, size);
    if (tmp > maxw) maxw = tmp;
  }

  SbBox2f maxbox;
  int numglyphs = this->glyphs.getLength();
  for (i = 0; i < numglyphs; i++) {
    maxbox.extendBy(this->glyphs[i]->getBoundingBox());
  }
  float maxglyphsize = maxbox.getMax()[1] - maxbox.getMin()[1];

  float maxy = size * maxbox.getMax()[1];
  float miny = maxy - (maxglyphsize*size + (n-1) * size * this->spacing.getValue());

  float minx, maxx;
  switch (this->justification.getValue()) {
  case SoAsciiText::LEFT:
    minx = 0.0f;
    maxx = maxw * size;
    break;
  case SoAsciiText::RIGHT:
    minx = -maxw * size;
    maxx = 0.0f;
    break;
  case SoAsciiText::CENTER:
    maxx = maxw * size * 0.5f;
    minx = -maxx;
    break;
  default:
    assert(0 && "unknown justification");
    minx = maxx = 0.0f;
    break;
  }

  box.setBounds(SbVec3f(minx, miny, 0.0f), SbVec3f(maxx, maxy, 0.0f));
  center = box.getCenter();
}

// Doc in parent.
void
SoAsciiText::generatePrimitives(SoAction * action)
{
  float size = SoFontSizeElement::get(action->getState());

  int i, n = this->string.getNum();

  SoPrimitiveVertex vertex;
  SoTextDetail detail;
  detail.setPart(0);
  vertex.setDetail(&detail);
  vertex.setMaterialIndex(0);

  this->beginShape(action, SoShape::TRIANGLES, NULL);
  vertex.setNormal(SbVec3f(0.0f, 0.0f, 1.0f));

  int glyphidx = 0;
  float ypos = 0.0f;

  for (i = 0; i < n; i++) {
    float currwidth = this->getWidth(i, size);
    detail.setStringIndex(i);
    float xpos = 0.0f;
    switch (this->justification.getValue()) {
    case SoAsciiText::RIGHT:
      xpos = -currwidth * size;
      break;
    case SoAsciiText::CENTER:
      xpos = - currwidth * size * 0.5f;
      break;
    }

    const char * str = this->string[i].getString();
    int len = strlen(str);
    float horizspacing = 0.0f;
    if (len > 1) {
      horizspacing = ((currwidth - this->glyphwidths[i]) / (len - 1)) * size;
    }
    int charidx = 0;

    while (*str++) {
      detail.setCharacterIndex(charidx++);
      const SoGlyph * glyph = this->glyphs[glyphidx++];
      const SbVec2f * coords = glyph->getCoords();
      const int * ptr = glyph->getFaceIndices();
      while (*ptr >= 0) {
        SbVec2f v0, v1, v2;
        v0 = coords[*ptr++];
        v1 = coords[*ptr++];
        v2 = coords[*ptr++];
        vertex.setTextureCoords(SbVec2f(v0[0], v0[1]));
        vertex.setPoint(SbVec3f(v0[0] * size + xpos, v0[1] * size + ypos, 0.0f));
        this->shapeVertex(&vertex);
        vertex.setTextureCoords(SbVec2f(v1[0], v1[1]));
        vertex.setPoint(SbVec3f(v1[0] * size + xpos, v1[1] * size + ypos, 0.0f));
        this->shapeVertex(&vertex);
        vertex.setTextureCoords(SbVec2f(v2[0], v2[1]));
        vertex.setPoint(SbVec3f(v2[0] * size + xpos, v2[1] * size + ypos, 0.0f));
        this->shapeVertex(&vertex);
      }
      xpos += glyph->getWidth() * size + horizspacing;
    }
    ypos -= size * this->spacing.getValue();
  }
  this->endShape();
}

// doc in parent
SbBool
SoAsciiText::willSetShapeHints(void) const
{
  return TRUE;
}

// doc in parent
SbBool
SoAsciiText::willUpdateNormalizeElement(SoState *) const
{
  return TRUE;
}

// doc in parent
SoDetail *
SoAsciiText::createTriangleDetail(SoRayPickAction * action,
                              const SoPrimitiveVertex * v1,
                              const SoPrimitiveVertex * v2,
                              const SoPrimitiveVertex * v3,
                              SoPickedPoint * pp)
{
  // generatePrimitives() places text details inside each primitive vertex
  assert(v1->getDetail());
  return v1->getDetail()->copy();
}


// recalculate glyphs
void
SoAsciiText::setUpGlyphs(SoState * state)
{
  if (!this->needsetup) return;
  this->needsetup = FALSE;

  // store old glyphs to avoid freeing glyphs too soon
  SbList <const SoGlyph *> oldglyphs;
  int i;
  int n = this->glyphs.getLength();
  for (i = 0; i < n; i++) {
    oldglyphs.append(this->glyphs[i]);
  }
  this->glyphs.truncate(0);
  this->glyphwidths.truncate(0);

  for (i = 0; i < this->string.getNum(); i++) {
    const SbString & s = this->string[i];
    int strlen = s.getLength();
    const char * ptr = s.getString();
    float width = 0.0f;
    for (int j = 0; j < strlen; j++) {
      const SoGlyph * glyph = SoGlyph::getGlyph(ptr[j], SbName("default"));
      this->glyphs.append(glyph);
      width += glyph->getWidth();
    }
    this->glyphwidths.append(width);
  }

  // unref old glyphs
  n = oldglyphs.getLength();
  for (i = 0; i < n; i++) {
    oldglyphs[i]->unref();
  }
}

// called whenever SoAsciiText::string is edited
void
SoAsciiText::fieldSensorCB(void * d, SoSensor *)
{
  SoAsciiText * thisp = (SoAsciiText *)d;
  thisp->needsetup = TRUE;
}

// returns "normalized" width of specified string. When too few
// width values are supplied, the glyphwidths are used instead.
float
SoAsciiText::getWidth(const int idx, const float fontsize)
{
  if (idx < this->width.getNum() && this->width[idx] > 0.0f)
    return this->width[idx] / fontsize;
  return this->glyphwidths[idx];
}
