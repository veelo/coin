/**************************************************************************\
 *
 *  This file is part of the Coin 3D visualization library.
 *  Copyright (C) 1998-2004 by Systems in Motion.  All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  ("GPL") version 2 as published by the Free Software Foundation.
 *  See the file LICENSE.GPL at the root directory of this source
 *  distribution for additional information about the GNU GPL.
 *
 *  For using Coin with software that can not be combined with the GNU
 *  GPL, and for taking advantage of the additional benefits of our
 *  support services, please contact Systems in Motion about acquiring
 *  a Coin Professional Edition License.
 *
 *  See <URL:http://www.coin3d.org/> for more information.
 *
 *  Systems in Motion, Teknobyen, Abels Gate 5, 7030 Trondheim, NORWAY.
 *  <URL:http://www.sim.no/>.
 *
\**************************************************************************/

#include "SoWriterefCounter.h"
#include <Inventor/SoOutput.h>
#include <Inventor/misc/SoBase.h>
#include <Inventor/SbDict.h>
#include <Inventor/C/threads/threadsutilp.h>
#include <Inventor/C/tidbits.h>
#include <Inventor/C/tidbitsp.h>
#include <Inventor/nodes/SoNode.h>
#include <assert.h>
#include <Inventor/errors/SoDebugError.h>


class SoWriterefCounterBaseData {
public:
  SoWriterefCounterBaseData() {
    writeref = 0;
    ingraph = FALSE;
  }

  int32_t writeref;
  SbBool ingraph;
};

class SoWriterefCounterOutputData {
public:
  SbDict writerefdict;

  SoWriterefCounterOutputData() 
    : writerefdict(1051), refcount(0) {
  }

  // need refcounter since dict can be shared among several SoOutputs
  void ref(void) {
    this->refcount++;
  }
  void unref(void) {
    if (--this->refcount == 0) {
      this->cleanup();
      delete this;
    }
  }
  void debugCleanup(void) {
    this->writerefdict.applyToAll(debug_dict);
    this->cleanup();
  }
  
protected:
  ~SoWriterefCounterOutputData() {  
  }

private:
  int refcount;

  void cleanup(void) {
    this->writerefdict.applyToAll(delete_dict_item);
    this->writerefdict.clear();
  }
  
  static void debug_dict(unsigned long key, void * value) {
#if COIN_DEBUG
    uintptr_t ptr = (uintptr_t) key;
    SoBase * base = (SoBase*) ((void*) ptr);
    SbName name = base->getName();
    if (name == "") name = "<noname>";
    
    SoDebugError::postWarning("SoWriterefCounter::<cleanup>",
                              "Not removed from writedict: %p, %s:%s",
                              base, base->getTypeId().getName().getString(), name.getString());
#endif // COIN_DEBUG
  }
  static void delete_dict_item(unsigned long key, void * value) { 
    SoWriterefCounterBaseData * data = (SoWriterefCounterBaseData*) value;
    delete data;
  }
};



class SoWriterefCounterP {
public:
  SoWriterefCounterP(SoWriterefCounter * master, SoOutput * out, SoWriterefCounterP * dataCopy) 
    : master(master), out(out)
  { 
    if (dataCopy) {
      this->outputdata = dataCopy->outputdata;
      this->sobase2id = new SbDict(*dataCopy->sobase2id);
    }
    else {
      this->outputdata = new SoWriterefCounterOutputData;
      this->sobase2id = new SbDict;
    }
    this->outputdata->ref();
    this->nextreferenceid = 0;
  }
  ~SoWriterefCounterP() {
    this->outputdata->unref();
  }
  SoWriterefCounter * master;
  SoOutput * out;
  SoWriterefCounterOutputData * outputdata;
  SbDict * sobase2id;
  int nextreferenceid;

  static void * mutex;
  static SbDict * outputdict;
  static SoWriterefCounter * current; // used to be backwards compatible
  static SbString * refwriteprefix;
  
  static void atexit_cleanup(void) {
    delete refwriteprefix;
    refwriteprefix = NULL;
    delete outputdict;
    outputdict = NULL;
    CC_MUTEX_DESTRUCT(mutex);
    mutex = NULL;
  }

};


void * SoWriterefCounterP::mutex;
SbDict *  SoWriterefCounterP::outputdict;
SoWriterefCounter *  SoWriterefCounterP::current; // used to be backwards compatible
SbString *  SoWriterefCounterP::refwriteprefix;


#define PRIVATE(obj) obj->pimpl



SoWriterefCounter::SoWriterefCounter(SoOutput * out, SoOutput * copyfrom)
{
  SoWriterefCounterP * datafrom = NULL;
  if (copyfrom) {
    SoWriterefCounter * frominst = SoWriterefCounter::instance(copyfrom);
    datafrom = frominst->pimpl;
  }
  PRIVATE(this) = new SoWriterefCounterP(this, out, datafrom);
}

SoWriterefCounter::~SoWriterefCounter()
{
  delete PRIVATE(this);
}

void 
SoWriterefCounter::create(SoOutput * out, SoOutput * copyfrom)
{
  SoWriterefCounter * inst = new SoWriterefCounter(out, copyfrom);
  uintptr_t ptr = (uintptr_t) out;
  CC_MUTEX_LOCK(SoWriterefCounterP::mutex);
  SbBool ret = SoWriterefCounterP::outputdict->enter((unsigned long) ptr, (void*) inst);
  assert(ret && "writeref instance already exists!");
  CC_MUTEX_UNLOCK(SoWriterefCounterP::mutex); 
}

void 
SoWriterefCounter::destruct(SoOutput * out)
{
  SoWriterefCounter * inst = SoWriterefCounter::instance(out);
  assert(inst && "instance not found!");
  uintptr_t ptr = (uintptr_t) out;

  CC_MUTEX_LOCK(SoWriterefCounterP::mutex);
  (void) SoWriterefCounterP::outputdict->remove((unsigned long) ptr);
  delete inst;
  CC_MUTEX_UNLOCK(SoWriterefCounterP::mutex); 
}

void
SoWriterefCounter::initClass(void)
{
  CC_MUTEX_CONSTRUCT(SoWriterefCounterP::mutex);
  SoWriterefCounterP::outputdict = new SbDict;
  SoWriterefCounterP::refwriteprefix = new SbString("+");
  coin_atexit((coin_atexit_f*) SoWriterefCounterP::atexit_cleanup, 0);
}

void 
SoWriterefCounter::debugCleanup(void)
{
  PRIVATE(this)->sobase2id->clear();
  PRIVATE(this)->outputdata->debugCleanup();
}

void 
SoWriterefCounter::setInstancePrefix(const SbString & s)
{
  (*SoWriterefCounterP::refwriteprefix) = s;
}

SoWriterefCounter * 
SoWriterefCounter::instance(SoOutput * out)
{
  if (out == NULL) {
    // to be backwards compatible with old code
    return SoWriterefCounterP::current;
  }

  CC_MUTEX_LOCK(SoWriterefCounterP::mutex);

  uintptr_t ptr = (uintptr_t) out;
  void * tmp;
  SoWriterefCounter * inst = NULL;
  
  if (SoWriterefCounterP::outputdict->find((unsigned long) ptr, tmp)) {
    inst = (SoWriterefCounter*) tmp;
  }
  else {
    assert(0 && "no instance");
  }
  SoWriterefCounterP::current = inst;
  CC_MUTEX_UNLOCK(SoWriterefCounterP::mutex);
  return inst;
}

SbBool 
SoWriterefCounter::shouldWrite(const SoBase * base) const
{
  uintptr_t ptr = (uintptr_t) base;
  void * tmp;
  
  if (PRIVATE(this)->outputdata->writerefdict.find((unsigned long)ptr, tmp)) {
    SoWriterefCounterBaseData * data = (SoWriterefCounterBaseData*) tmp;
    return data->ingraph;
  }
  return FALSE;
}

SbBool 
SoWriterefCounter::hasMultipleWriteRefs(const SoBase * base) const
{
  uintptr_t ptr = (uintptr_t) base;
  void * tmp;
  
  if (PRIVATE(this)->outputdata->writerefdict.find((unsigned long)ptr, tmp)) {
    SoWriterefCounterBaseData * data = (SoWriterefCounterBaseData*)tmp;
    return data->writeref > 1;
  }
  return FALSE;
}

int 
SoWriterefCounter::getWriteref(const SoBase * base) const
{
  uintptr_t ptr = (uintptr_t) base;
  void * tmp;
  
  if (PRIVATE(this)->outputdata->writerefdict.find((unsigned long)ptr, tmp)) {
    SoWriterefCounterBaseData * data = (SoWriterefCounterBaseData*)tmp;
    return data->writeref;
  }
  return 0;
}

void 
SoWriterefCounter::setWriteref(const SoBase * base, const int ref)
{
  uintptr_t ptr = (uintptr_t) base;
  void * tmp;
  
  // for debugging
  //  SbName name = base->getName();
  //  fprintf(stderr,"writeref: %s, %d, ingraph: %d\n", name.getString(), ref,
  //          isInGraph(base));
  //   }

  if (PRIVATE(this)->outputdata->writerefdict.find((unsigned long)ptr, tmp)) {
    SoWriterefCounterBaseData * data = (SoWriterefCounterBaseData*)tmp;
    data->writeref = ref;
  }
  else {
    SoWriterefCounterBaseData * data = new SoWriterefCounterBaseData;
    data->writeref = ref;
    (void) PRIVATE(this)->outputdata->writerefdict.enter((unsigned long)ptr, (void*)data);
  }


  if (ref == 0) {
    SoOutput * out = PRIVATE(this)->out;
    // for better debugging
    this->removeWriteref(base);
    // Ouch. Does this to avoid having two subsequent write actions on
    // the same SoOutput to write "USE ..." when it should write a
    // full node/subgraph specification on the second run.  -mortene.
    //
    // FIXME: accessing out->removeSoBase2IdRef() directly takes a
    // "friend SoBase" in the SoOutput class definition. Should fix
    // with proper design for next major Coin release. 20020426 mortene.
    if (out->findReference(base) != FIRSTWRITE) {
      out->removeSoBase2IdRef(base);
    }
  }
  if (ref < 0) {
    SbName name = base->getName();
    if (name == "") name = "<noname>";
    SoDebugError::postWarning("SoWriterefCounter::setWriteref",
                              "writeref < 0 for %s <%p>", name.getString(), base);
  }
}
  
void 
SoWriterefCounter::decrementWriteref(const SoBase * base)
{
  this->setWriteref(base, this->getWriteref(base) - 1);
}


SbBool 
SoWriterefCounter::isInGraph(const SoBase * base) const
{
  uintptr_t ptr = (uintptr_t) base;
  void * tmp;
  
  if (PRIVATE(this)->outputdata->writerefdict.find((unsigned long)ptr, tmp)) {
    SoWriterefCounterBaseData * data = (SoWriterefCounterBaseData*)tmp;
    return data->ingraph;
  }
  return FALSE;
}

void 
SoWriterefCounter::setInGraph(const SoBase * base, const SbBool ingraph)
{
  uintptr_t ptr = (uintptr_t) base;
  void * tmp;
  
  if (PRIVATE(this)->outputdata->writerefdict.find((unsigned long)ptr, tmp)) {
    SoWriterefCounterBaseData * data = (SoWriterefCounterBaseData*)tmp;
    data->ingraph = ingraph;
  }
  else {
    SoWriterefCounterBaseData * data = new SoWriterefCounterBaseData;
    data->ingraph = ingraph;
    (void) PRIVATE(this)->outputdata->writerefdict.enter((unsigned long)ptr, (void*)data);
  }
}

void 
SoWriterefCounter::removeWriteref(const SoBase * base) 
{ 
  uintptr_t ptr = (uintptr_t) base;
  void * tmp;
  
  if (PRIVATE(this)->outputdata->writerefdict.find((unsigned long)ptr, tmp)) {
    SoWriterefCounterBaseData * data = (SoWriterefCounterBaseData*)tmp;
    delete data;
    (void) PRIVATE(this)->outputdata->writerefdict.remove((unsigned long)ptr);
  }
  else {
    assert(0 && "writedata not found");
  }
}

//
// If this environment variable is set to 1, we try to preserve
// the original node names as far as possible instead of appending
// a "+<refid>" suffix.
//
static SbBool
dont_mangle_output_names(const SoBase *base)
{
  static int COIN_DONT_MANGLE_OUTPUT_NAMES = -1;

  // Always unmangle node names in VRML1 and VRML2
  if (base->isOfType(SoNode::getClassTypeId()) &&
      (((SoNode *)base)->getNodeType()==SoNode::VRML1 ||
       ((SoNode *)base)->getNodeType()==SoNode::VRML2)) return TRUE;

  if (COIN_DONT_MANGLE_OUTPUT_NAMES < 0) {
    COIN_DONT_MANGLE_OUTPUT_NAMES = 0;
    const char * env = coin_getenv("COIN_DONT_MANGLE_OUTPUT_NAMES");
    if (env) COIN_DONT_MANGLE_OUTPUT_NAMES = atoi(env);
  }
  return COIN_DONT_MANGLE_OUTPUT_NAMES ? TRUE : FALSE;
}

SbName 
SoWriterefCounter::getWriteName(const SoBase * base) const
{
  SoOutput * out = PRIVATE(this)->out;
  SbName name = base->getName();
  int refid = out->findReference(base);
  SbBool firstwrite = (refid == (int) FIRSTWRITE);
  SbBool multiref = this->hasMultipleWriteRefs(base);
  
  // Find what node name to write
  SbString writename;
  if (dont_mangle_output_names(base)) {
    //
    // Try to keep the original node names as far as possible.
    // Weaknesses (FIXME kintel 20020429):
    //  o We should try to reuse refid's as well.
    //  o We should try to let "important" (=toplevel?) nodes
    //    keep their original node names before some subnode "captures" it.
    //

    /* Code example. The correct output file is shown below

       #include <Inventor/SoDB.h>
       #include <Inventor/SoInput.h>
       #include <Inventor/SoOutput.h>
       #include <Inventor/actions/SoWriteAction.h>
       #include <Inventor/nodes/SoSeparator.h>

       void main(int argc, char *argv[])
       {
       SoDB::init();

       SoSeparator *root = new SoSeparator;
       root->ref();
       root->setName("root");

       SoSeparator *n0 = new SoSeparator;
       SoSeparator *a0 = new SoSeparator;
       SoSeparator *a1 = new SoSeparator;
       SoSeparator *a2 = new SoSeparator;
       SoSeparator *a3 = new SoSeparator;
       SoSeparator *b0 = new SoSeparator;
       SoSeparator *b1 = new SoSeparator;
       SoSeparator *b2 = new SoSeparator;
       SoSeparator *b3 = new SoSeparator;
       SoSeparator *b4 = new SoSeparator;
       SoSeparator *c0 = new SoSeparator;

       a2->setName(SbName("MyName"));
       b0->setName(SbName("MyName"));
       b1->setName(SbName("MyName"));
       b2->setName(SbName("MyName"));
       b4->setName(SbName("MyName"));
       c0->setName(SbName("MyName"));

       root->addChild(n0);
       root->addChild(n0);
       root->addChild(a0);
       a0->addChild(b0);
       a0->addChild(b1);
       root->addChild(b0);
       root->addChild(a1);
       a1->addChild(b2);
       a1->addChild(b1);
       root->addChild(b1);
       root->addChild(a2);
       root->addChild(a2);
       root->addChild(a3);
       a3->addChild(b3);
       b3->addChild(c0);
       b3->addChild(c0);
       a3->addChild(b4);
       a3->addChild(a2);

       SoOutput out;
       out.openFile("out.wrl");
       out.setHeaderString(SbString("#VRML V1.0 ascii"));
       SoWriteAction wra(&out);
       wra.apply(root);
       out.closeFile();

       root->unref();
       }

       Output file:

       #VRML V1.0 ascii

       DEF root Separator {
         DEF +0 Separator {
         }
         USE +0
         Separator {
           DEF MyName Separator {
           }
           DEF MyName+1 Separator {
           }
         }
         USE MyName
         Separator {
           DEF MyName Separator {
           }
           USE MyName+1
         }
         USE MyName+1
         DEF MyName Separator {
         }
         USE MyName
         Separator {
           Separator {
             DEF MyName+2 Separator {
             }
             USE MyName+2
           }
           DEF MyName+3 Separator {
           }
           USE MyName
         }
       }
    */

    if (!firstwrite) {
      writename = name.getString();
      // We have used a suffix when DEF'ing the node
      if (refid != (int) NOSUFFIX) {
        writename += SoWriterefCounterP::refwriteprefix->getString();
        writename.addIntString(refid);
      }
      // Detects last USE of a node, enables reuse of DEF's
      if (!multiref) out->removeDEFNode(SbName(writename));
    }
    else {
      SbBool found = out->lookupDEFNode(name);
      writename = name.getString();
      if (!found && (!multiref || name.getLength() > 0)) {
        // We can use the node without a suffix
        if (multiref) out->addDEFNode(name);
        out->setReference(base, (int) NOSUFFIX);
      }
      else {
        // Node name is already DEF'ed or an unnamed multiref => use a suffix.
        writename += SoWriterefCounterP::refwriteprefix->getString();
        writename.addIntString(out->addReference(base));
        out->addDEFNode(SbName(writename));
      }
    }
  }
  else { // Default OIV behavior
    if (multiref && firstwrite) refid = out->addReference(base);
    if (!firstwrite) {
      writename = name.getString();
      writename += SoWriterefCounterP::refwriteprefix->getString();
      writename.addIntString(refid);
    }
    else {
      writename = name.getString();
      if (multiref) {
        writename += SoWriterefCounterP::refwriteprefix->getString();
        writename.addIntString(refid);
      }
    }
  }
  return writename;
}

/*!
  Makes a unique id for \a base and adds a mapping into our dictionary.
*/
int
SoWriterefCounter::addReference(const SoBase * base)
{
  if (!PRIVATE(this)->sobase2id) PRIVATE(this)->sobase2id = new SbDict;
  int id = PRIVATE(this)->nextreferenceid++;
  // Ugly! Should be solved by making a decent templetized
  // SbDict-alike class.
  PRIVATE(this)->sobase2id->enter((unsigned long)base, (void *)id);
  return id;
}

/*!
  Returns the unique identifier for \a base or -1 if not found.
*/
int
SoWriterefCounter::findReference(const SoBase * base) const
{
  // Ugly! Should be solved by making a decent templetized
  // SbDict-alike class.
  void * id;
  SbBool ok = PRIVATE(this)->sobase2id && PRIVATE(this)->sobase2id->find((unsigned long)base, id);
  // the extra intermediate "long" cast is needed by 64-bits IRIX CC
  return ok ? (int)((long)id) : -1;
}

/*!
  Sets the reference for \a base manually.
*/
void
SoWriterefCounter::setReference(const SoBase * base, int refid)
{
  if (!PRIVATE(this)->sobase2id) PRIVATE(this)->sobase2id = new SbDict;
  PRIVATE(this)->sobase2id->enter((unsigned long)base, (void *)refid);
}

void
SoWriterefCounter::removeSoBase2IdRef(const SoBase * base)
{
  PRIVATE(this)->sobase2id->remove((unsigned long)base);
}


#undef PRIVATE