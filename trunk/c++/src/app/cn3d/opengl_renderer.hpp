/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Paul Thiessen
*
* File Description:
*      Classes to hold the OpenGL rendering engine
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2000/08/18 18:57:43  thiessen
* added transparent spheres
*
* Revision 1.13  2000/08/17 14:22:00  thiessen
* added working StyleManager
*
* Revision 1.12  2000/08/13 02:42:13  thiessen
* added helix and strand objects
*
* Revision 1.11  2000/08/11 12:59:13  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.10  2000/08/07 14:12:48  thiessen
* added animation frames
*
* Revision 1.9  2000/08/07 00:20:18  thiessen
* add display list mechanism
*
* Revision 1.8  2000/08/04 22:49:10  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.7  2000/08/03 15:12:29  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.6  2000/07/18 00:05:45  thiessen
* allow arbitrary rotation center
*
* Revision 1.5  2000/07/17 22:36:46  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.4  2000/07/17 04:21:09  thiessen
* now does correct structure alignment transformation
*
* Revision 1.3  2000/07/16 23:18:34  thiessen
* redo of drawing system
*
* Revision 1.2  2000/07/12 23:28:27  thiessen
* now draws basic CPK model
*
* Revision 1.1  2000/07/12 14:10:45  thiessen
* added initial OpenGL rendering engine
*
* ===========================================================================
*/

#ifndef CN3D_OPENGL_RENDERER__HPP
#define CN3D_OPENGL_RENDERER__HPP

// do not include GL headers here, so that other modules can more easily
// access this without potential name conflicts

#include <list>
#include <map>

#include "cn3d/vector_math.hpp"


BEGIN_SCOPE(Cn3D)

// the OpenGLRenderer class

class StructureSet;
class AtomStyle;
class BondStyle;
class HelixStyle;
class StrandStyle;

class OpenGLRenderer
{
public:
    OpenGLRenderer(void);

    // public data
    static const unsigned int NO_NAME;

    // public methods

    // calls once-only OpenGL initialization stuff (should be called after
    // the rendering context is established and the renderer made current)
    void Init(void) const;

    // sets the size of the viewport
    void SetSize(int width, int height) const;

    // tells the renderer that new camera settings need to be applied
    void NewView(int selectX = -1, int selectY = -1) const;

    // get the name 
    bool OpenGLRenderer::GetSelected(int x, int y, unsigned int *name);

    // reset camera to original state
    void ResetCamera(void);

    // called to change view (according to mouse movements)
    enum eViewAdjust {
        eXYRotateHV,        // rotate about X,Y axes according to horiz. & vert. movement
        eZRotateH,          // rotate in plane (about Z) according to horiz. movement
        eXYTranslateHV,     // translate in X,Y according to horiz. & vert. movement
        eZoomHHVV,          // zoom according to (H1,V1),(H2,V2) box
        eZoomIn,            // zoom in
        eZoomOut,           // zoom out
        eCenterCamera       // reset camera to look at origin
    };
    void ChangeView(eViewAdjust control, int dX = 0, int dY = 0, int X2 = 0, int Y2 = 0);

    // draws the display lists
    void Display(void);

    // tells the renderer what structure(s) it's to draw
    void AttachStructureSet(StructureSet *targetStructureSet);

    // constructs the structure display lists (but doesn't draw them)
    void Construct(void);

    // push the global view matrix, then apply transformation (e.g., for structure alignment)
    void PushMatrix(const Matrix* xform);
    // pop matrix
    void PopMatrix(void);

    // display list management
    static const unsigned int NO_LIST, FIRST_LIST;
    void StartDisplayList(unsigned int list);
    void EndDisplayList(void);

    // frame management
    void ShowAllFrames(void);
    void ShowFirstFrame(void);
    void ShowLastFrame(void);
    void ShowNextFrame(void);
    void ShowPreviousFrame(void);

    // drawing methods
    void DrawAtom(const Vector& site, const AtomStyle& atomStyle, double alpha);
    void DrawBond(const Vector& site1, const Vector& site2, const BondStyle& style,
        const Vector *site0, const Vector* site3);
    void DrawHelix(const Vector& Nterm, const Vector& Cterm, const HelixStyle& helixStyle);
    void DrawStrand(const Vector& Nterm, const Vector& Cterm, 
        const Vector& unitNormal, const StrandStyle& strandStyle);

private:
    StructureSet *structureSet;
    void SetColor(int type, float red = 0.0, float green = 0.0, float blue = 0.0, float alpha = 1.0);
    void ConstructLogo(void);

    // camera data
    double cameraDistance, cameraAngleRad, 
        cameraLookAtX, cameraLookAtY,
        cameraClipNear, cameraClipFar,
        viewMatrix[16];

    // controls for view changes
    double rotateSpeed;

    // misc rendering stuff
    bool selectMode;
    unsigned int currentFrame;

    // Quality settings - will eventually be stored in program registry
    int atomSlices, atomStacks;     // for atom spheres
    int bondSides;                  // for bonds
    int wormSides, wormSegments;    // for worm bonds
    int helixSides;                 // for helix objects

    // stuff for storing transparent spheres (done during Construct())
    unsigned int currentDisplayList;
    Matrix currentSlaveTransform;
    typedef struct {
        Vector site, color; 
        unsigned int name;
        double radius, alpha;
    } SphereInfo;
    typedef std::list < SphereInfo > SphereList;
    typedef std::pair < SphereList, Matrix > SphereListAndTransform;
    typedef std::map < unsigned int, SphereListAndTransform > SphereMap;
    SphereMap sphereMap;
    void AddTransparentSphere(const Vector& color, unsigned int name,
        const Vector& site, double radius, double alpha);
    void ClearTransparentSpheres(void) { sphereMap.clear(); }

    // stuff for rendering transparent spheres (done during Display())
    class SpherePtr
    {
    public:
        Vector siteGL; // atom site in GL coordinates
        double distanceFromCamera;
        SphereInfo *ptr;
        friend bool operator < (const SpherePtr& a, const SpherePtr& b)
            { return (a.distanceFromCamera < b.distanceFromCamera); }
    };
    typedef std::list < SpherePtr > SpherePtrList;
    SpherePtrList renderSphereList;
    void AddTransparentSpheresFromList(unsigned int list);
    void RenderTransparentSpheres(void);
};

END_SCOPE(Cn3D)

#endif // CN3D_OPENGL_RENDERER__HPP
