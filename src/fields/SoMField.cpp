/**************************************************************************\
 *
 *  Copyright (C) 1998-2000 by Systems in Motion.  All rights reserved.
 *
 *  This file is part of the Coin library.
 *
 *  This file may be distributed under the terms of the Q Public License
 *  as defined by Troll Tech AS of Norway and appearing in the file
 *  LICENSE.QPL included in the packaging of this file.
 *
 *  If you want to use Coin in applications not covered by licenses
 *  compatible with the QPL, you can contact SIM to aquire a
 *  Professional Edition license for Coin.
 *
 *  Systems in Motion AS, Prof. Brochs gate 6, N-7030 Trondheim, NORWAY
 *  http://www.sim.no/ sales@sim.no Voice: +47 22114160 Fax: +47 67172912
 *
\**************************************************************************/

/*!
  \class SoMField SoMField.h Inventor/fields/SoMField.h
  \brief The SoMField class is the base class for fields which can contain multiple values.
  \ingroup fields

  All field types which may contain more than one member
  value inherits this class. SoMField is an abstract class.

  Use setValue(), setValues() or set1Value() to set the values of
  fields inheriting SoMField, and use getValues() or the index
  operator [] to read values. Example code:

  \code
    SoText2 * textnode = new SoText2;
    textnode->ref();

    /////// Setting multi-field values. /////////////////////////////

    // Set the first value of the SoMFString field of the SoText2 node.
    // The field array will be truncated to only contain this single value.
    textnode->string.setValue("Morten");
    // The setValue() method and the = operator is interchangeable,
    // so this code line does the same as the previous line.
    textnode->string = "Peder";

    // Set the value at index 2. If the field value array contained
    // fewer than 3 elements before this call, first expand it to contain
    // 3 elements.
    textnode->string.set1Value(2, "Lars");

    // This sets 3 values of the array, starting at index 5. If the
    // array container fewer than 8 elements before the setValues()
    // call, the array will first be expanded.
    SbString s[3] = { "Eriksen", "Blekken", "Aas" };
    textnode->string.setValues(5, sizeof(s), s);


    /////// Inspecting multi-field values. //////////////////////////

    // This will read the second element (counting from zero) if the
    // multivalue field and place it in "val".
    SbString val = textnode->string[2];

    // Gives us a pointer to the array which the multiple-value field
    // is using to store the values. Note that the return value is
    // "const", so you can only read from the array, not write to
    // it.
    const SbString * vals = textnode->string.getValues(0);


    /////// Modifying multi-field values. ///////////////////////////

    // You can of course modify multifield-values by using the set-
    // and get-methods shown above, but when you're working with
    // big sets of data, this will be ineffective. Then use this
    // technique instead:
    SbString * modvals = textnode->string.startEditing();

    // ... lots of modifications to the "modvals" array here ...

    // Calling the finishEditing() method is necessary for the
    // scene graph to be updated (and re-rendered).
    textnode->string.finishEditing();

  \endcode


  When nodes, engines or other types of field containers are written
  to file, their multiple-value fields are written to file in this
  format:

  \code
    containerclass {
      fieldname [ value0, value1, value2, ...]
      ...
    }
  \endcode

  ..like this, for instance, a Coordinate3 node providing 6 vertex
  coordinates in the form of SbVec3f values in its "point" field for
  e.g. a faceset, lineset or pointset:

  \code
     Coordinate3 {
        point [
           -1 1 0, -1 -1 0, 1 -1 0,
           0 2 -1, -2 0 -1, 0 -2 -1,
        ]
     }
  \endcode


  \sa SoSField

 */

#include <Inventor/fields/SoMField.h>
#include <Inventor/SbName.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/errors/SoReadError.h>
#include <Inventor/fields/SoSubField.h>
#include <Inventor/misc/SoBasic.h> // COIN_STUB()
#include <assert.h>
#include <malloc.h>

#if COIN_DEBUG
#include <Inventor/errors/SoDebugError.h>
#endif // COIN_DEBUG

/*!
  \var int SoMField::num
  Number of available values.
*/
/*!
  \var int SoMField::maxNum
  Number of array "slots" allocated for this field.
*/


SoType SoMField::classTypeId = SoType::badType();

// Overridden from parent class.
SoType
SoMField::getClassTypeId(void)
{
  return SoMField::classTypeId;
}

// Overridden from parent class.
void
SoMField::initClass(void)
{
  PRIVATE_FIELD_INIT_CLASS(SoMField, "MField", inherited, NULL);
}

/*!
  Constructor. Initializes number of values in field to zero.
*/
SoMField::SoMField(void)
{
  this->maxNum = this->num = 0;
}

/*!
  Destructor in SoMField does nothing. Resource deallocation needs to
  be done from subclasses.
*/
SoMField::~SoMField()
{
}

/*!
  Make room in the field to store \a newnum values.
*/
void
SoMField::makeRoom(int newnum)
{
  assert(newnum >= 0);
  if (newnum != num) this->allocValues(newnum);
}

/*!
  Set the value at \a index to the value contained in \a valstr.
  Returns \c TRUE if a valid value for this field can be extracted
  from \a valstr, otherwise \c FALSE.
*/
SbBool
SoMField::set1(const int index, const char * const valstr)
{
  SoInput in;
  in.setBuffer((void *)valstr, strlen(valstr));
  if (!this->read1Value(&in, index)) return FALSE;
  this->valueChanged();
  return TRUE;
}

/*!
  Return the value at \a index in the \a valstr string.
*/
void
SoMField::get1(const int index, SbString & valstr)
{
  SoOutput out;
  out.setHeaderString("");

  void * buffer = malloc(128);
  out.setBuffer(buffer, 10, realloc);

  this->evaluate();
  this->write1Value(&out, index);

  size_t dummy;
  out.getBuffer(buffer, dummy);

  valstr = (char *)buffer;
  free(buffer);
}

/*!
  Read and set all values for this field from input stream \a in.
  Returns \c TRUE if import went ok, otherwise \c FALSE.
*/
SbBool
SoMField::readValue(SoInput * in)
{
  // This macro is convenient for reading with error detection.
#define READ_VAL(val) \
  if (!in->read(val)) { \
    SoReadError::post(in, "Premature end of file"); \
    return FALSE; \
  }


  if (in->isBinary()) {
    int numtoread;
    READ_VAL(numtoread);

    // Sanity checking on the value, to avoid barfing on corrupt
    // files.
    if (numtoread < 0) {
      SoReadError::post(in, "invalid number of values in field: %d",
                        numtoread);
      return FALSE;
    }
    else if (numtoread > 32768) {
      SoReadError::post(in, "%d values in field, file probably corrupt",
                        numtoread);
      return FALSE;
    }

    this->makeRoom(numtoread);
    return this->readBinaryValues(in, numtoread);
  }

  char c;
  READ_VAL(c);
  if (c == '[') {
    int currentidx = 0;

    READ_VAL(c);
    if (c == ']') {
      // Zero values -- done. :^)
    }
    else {
      in->putBack(c);

      while (TRUE) {
        // makeRoom() makes sure the allocation strategy is decent.
        if (currentidx >= this->num) this->makeRoom(currentidx + 1);

        if (!this->read1Value(in, currentidx++)) return FALSE;

        READ_VAL(c);
        if (c == ',') {
          READ_VAL(c);
          if (c == ']') break;
          else in->putBack(c);
        }
        else if (c == ']')
          break;
        else
          in->putBack(c);
      }
    }

    // Fit array to number of items.
    this->makeRoom(currentidx);
  }
  else {
    in->putBack(c);
    this->makeRoom(1);
    if (!this->read1Value(in, 0)) return FALSE;
  }

#undef READ_VAL

  return TRUE;
}

/*!
  Write all field values to \a out.
*/
void
SoMField::writeValue(SoOutput * out) const
{
  if (out->isBinary()) {
    this->writeBinaryValues(out);
    return;
  }

  SbBool indented = FALSE;

  const int num = this->getNum();
  if ((num > 1) || (num == 0)) out->write("[ ");

  for (int i=0; i < num; i++) {
    this->write1Value(out, i);

    if (i != num-1) {
      if (((i+1) % this->getNumValuesPerLine()) == 0) {
        out->write(",\n");
        if (!indented) {
          out->incrementIndent();
          indented = TRUE;
        }
        out->indent();
        // for alignment
        out->write("  ");
      }
      else {
        out->write(", ");
      }
    }
  }
  if ((num > 1) || (num == 0)) out->write(" ]");

  if (indented) out->decrementIndent();
}

/*!
  Read \a num binary format values from \a in into this field.
*/
SbBool
SoMField::readBinaryValues(SoInput * in, int num)
{
  assert(in->isBinary());
  assert(num >= 0);

  for (int i=0; i < num; i++) if (!this->read1Value(in, i)) return FALSE;
  return TRUE;
}

/*!
  Write all values of field to \a out in binary format.
*/
void
SoMField::writeBinaryValues(SoOutput * out) const
{
  assert(out->isBinary());

  const int num = this->getNum();
  out->write(num);
  for (int i=0; i < num; i++) this->write1Value(out, i);
}

// Number of values written to each line during export to ASCII format
// files. Overload this in subclasses for prettier formating.
int
SoMField::getNumValuesPerLine(void) const
{
  return 1;
}

/*!
  Returns number of values in this field.
*/
int
SoMField::getNum(void) const
{
  this->evaluate();
  return this->num;
}

/*!
  Set number of values to \a num.

  If the current number of values is less than \a num, the array of
  values will be truncated from the end, if \a num is larger than the
  current number of values, random values will be appended.
 */
void
SoMField::setNum(const int num)
{
  this->allocValues(num);
}

/*!
  Remove value elements from index \a start up to and including index
  \a start + \a num - 1.

  Elements with indices larger than the last deleted element will
  be moved downwards in the value array.

  If \a num equals -1, delete from \a start and to the end of the array.
*/
void
SoMField::deleteValues(int start, int num)
{
  if (num == -1) num = this->getNum() - start;
  if (num == 0) return;
  int end = start + num; // First element behind the delete block.

#if COIN_DEBUG
  if (start < 0 || start >= this->getNum() || end > this->getNum() || num < -1) {
    SoDebugError::post("SoMField::deleteValues",
                       "invalid indices [%d, %d] for array of size %d",
                       start, end - 1, this->getNum());
    return;
  }
#endif // COIN_DEBUG

  // Move elements downward to fill the gap.
  if (end != this->getNum()) {
    int fsize = this->fieldSizeof();
    char * move_to = ((char *)this->valuesPtr()) + start * fsize;
    char * move_from = ((char *)this->valuesPtr()) + end * fsize;
    int elements = this->getNum() - end;

    (void)memmove(move_to, move_from, fsize * elements);
  }

  // Truncate.
  this->setNum(this->getNum() - num);
}

/*!
  Insert \a num "slots" for new value elements from \a start.
  The elements already present from \a start will be moved
  "upward" in the extended array.
*/
void
SoMField::insertSpace(int start, int num)
{
  if (num == 0) return;

#if COIN_DEBUG
  if (start < 0 || start > this->getNum() || num < 0) {
    SoDebugError::post("SoMField::insertSpace",
                       "invalid indices [%d, %d] for array of size %d",
                       start, start + num, this->getNum());
    return;
  }
#endif // COIN_DEBUG

  // Expand array.
  this->setNum(this->getNum() + num);

  // Copy values upward.
  if (start < this->getNum()) {
    int fsize = this->fieldSizeof();
    char * move_from = ((char *)this->valuesPtr()) + start * fsize;
    char * move_to = ((char *)this->valuesPtr()) + (start + num) * fsize;
    (void)memcpy(move_to, move_from, fsize * num);
  }
}

// If there's any multiple value fields without conversion
// functionality, calls to convertTo() will end up here.
void
SoMField::convertTo(SoField * dest) const
{
#if COIN_DEBUG
  SoDebugError::postWarning("SoMField::convertTo",
                            "Can't convert from %s to %s",
                            this->getTypeId().getName().getString(),
                            dest->getTypeId().getName().getString());
#endif // COIN_DEBUG
}

#ifndef DOXYGEN_SKIP_THIS // Internal method.
void
SoMField::allocValues(int newnum)
{
  assert(newnum >= 0);

  if (newnum == 0) {
    delete (unsigned char *) this->valuesPtr();
    this->setValuesPtr(NULL);
    this->maxNum = 0;
  }
  else if (newnum > this->maxNum || newnum < this->num) {
    int fsize = this->fieldSizeof();
    if (this->valuesPtr()) {

      // I think this will handle both cases quite gracefully
      // 1) newnum > this->maxNum, 2) newnum < num
      while (newnum > this->maxNum) this->maxNum<<=1;
      while ((this->maxNum >> 1) >= newnum) this->maxNum >>= 1;

#if 0 // debug
      SoDebugError::postInfo("SoMField::allocValues",
                             "%d --> %d, name: '%s', ptr: %p",
                             newnum, this->maxNum,
                             getTypeId().getName().getString(), this);
#endif // debug


      unsigned char * newblock = new unsigned char[this->maxNum * fsize];

      (void)memcpy(newblock, this->valuesPtr(),
                   fsize * SbMin(this->num, newnum));

      delete (unsigned char *) this->valuesPtr();
      this->setValuesPtr(newblock);
    }
    else {
      this->setValuesPtr(new unsigned char[newnum * fsize]);
      this->maxNum = newnum;
    }
  }

  SbBool valchanged = newnum < this->num ? TRUE : FALSE;
  this->num = newnum;
  if (valchanged) this->valueChanged();
}
#endif // DOXYGEN_SKIP_THIS
