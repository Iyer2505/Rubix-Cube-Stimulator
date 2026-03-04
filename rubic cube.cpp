// rubik_fixed.cpp
// Full working Rubik's Cube simulator (single-file)
// Requires FreeGLUT. Compile with g++ and link to freeglut, opengl32, glu32.
//
// Keyboard mappings:
//  a / q : Top (U) clockwise / anti-clockwise
//  s / w : Right (R)
//  d / e : Front (F)
//  f / r : Left (L)
//  g / t : Back (B)
//  h / y : Bottom (D)
//  Arrow keys also do U/L/R/D rotations (Up=U, Down=D, Left=L, Right=R) - clockwise
//  Numbers 1/2/4/5/6/8/9 rotate camera/view
//  m / n : increase / decrease rotation speed
//  o : undo last move (creates reverse move animation)
//  ESC : exit
//
// Mouse:
//  Left-button drag rotates the camera.

#include <GL/freeglut.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <deque>

#define PI 3.14159265358979323846f

// Face indices
enum Face { U = 0, R = 1, F = 2, B = 3, L = 4, D = 5 };

// Face colors (RGB)
static const GLfloat FACE_COLORS[6][3] = {
    {1.0f, 1.0f, 1.0f}, // U - white
    {1.0f, 0.5f, 0.0f}, // R - orange
    {0.0f, 0.0f, 1.0f}, // F - blue
    {0.0f, 1.0f, 0.0f}, // B - green
    {1.0f, 1.0f, 0.0f}, // L - yellow
    {1.0f, 0.0f, 0.0f}  // D - red
};

struct Move {
    int face;   // Face index (U,R,F,B,L,D)
    int dir;    // +1 clockwise, -1 counterclockwise (from that face's perspective)
    Move() : face(0), dir(1) {}
    Move(int f, int d) : face(f), dir(d) {}
};

// Cube state: 6 faces, each 3x3, store color index (0..5)
int cube[6][3][3];

// Animation / runtime state
bool animating = false;
int animFace = -1;        // which face is currently animating
int animDir = 1;          // 1 or -1
float animAngle = 0.0f;   // current animation angle in degrees
float animSpeed = 6.0f;   // degrees per frame (adjust with m/n)
std::deque<Move> moveHistory; // recorded moves for undo

// Camera view
float viewYaw = -30.0f;   // q (left-right)
float viewPitch = 20.0f;  // p (up-down)
int lastMouseX = 0, lastMouseY = 0;
bool mouseDown = false;

// Utility: clamp
template<typename T>
T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Initialize solved cube
void initCube() {
    for (int f = 0; f < 6; ++f)
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                cube[f][i][j] = f;
}

// rotate a 3x3 face matrix clockwise
void rotateFaceCW(int faceArr[3][3]) {
    int tmp[3][3];
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) tmp[i][j] = faceArr[i][j];
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) faceArr[i][j] = tmp[2-j][i];
}

// rotate a 3x3 face matrix counter-clockwise
void rotateFaceCCW(int faceArr[3][3]) {
    // three CW rotations
    rotateFaceCW(faceArr);
    rotateFaceCW(faceArr);
    rotateFaceCW(faceArr);
}

// Apply a logical move to the cube state (no animation)
void applyMoveToState(int f, int dir) {
    // dir = +1 : clockwise when looking at that face
    // We'll implement swaps for each face.
    // Indexing convention: face[f][row][col] with row=0 top, row=2 bottom; col=0 left, col=2 right
    int temp[3];
    if (f == U) {
        // rotate U face itself
        if (dir==1) rotateFaceCW(cube[U]);
        else rotateFaceCCW(cube[U]);
        // sides: F, R, B, L top rows cycle
        if (dir == 1) {
            for (int i=0;i<3;++i) temp[i] = cube[F][0][i];
            for (int i=0;i<3;++i) cube[F][0][i] = cube[R][0][i];
            for (int i=0;i<3;++i) cube[R][0][i] = cube[B][0][i];
            for (int i=0;i<3;++i) cube[B][0][i] = cube[L][0][i];
            for (int i=0;i<3;++i) cube[L][0][i] = temp[i];
        } else {
            for (int i=0;i<3;++i) temp[i] = cube[F][0][i];
            for (int i=0;i<3;++i) cube[F][0][i] = cube[L][0][i];
            for (int i=0;i<3;++i) cube[L][0][i] = cube[B][0][i];
            for (int i=0;i<3;++i) cube[B][0][i] = cube[R][0][i];
            for (int i=0;i<3;++i) cube[R][0][i] = temp[i];
        }
    } else if (f == D) {
        if (dir==1) rotateFaceCW(cube[D]); else rotateFaceCCW(cube[D]);
        // bottom rows cycle: F,R,B,L bottom rows
        if (dir==1) {
            for (int i=0;i<3;++i) temp[i] = cube[F][2][i];
            for (int i=0;i<3;++i) cube[F][2][i] = cube[L][2][i];
            for (int i=0;i<3;++i) cube[L][2][i] = cube[B][2][i];
            for (int i=0;i<3;++i) cube[B][2][i] = cube[R][2][i];
            for (int i=0;i<3;++i) cube[R][2][i] = temp[i];
        } else {
            for (int i=0;i<3;++i) temp[i] = cube[F][2][i];
            for (int i=0;i<3;++i) cube[F][2][i] = cube[R][2][i];
            for (int i=0;i<3;++i) cube[R][2][i] = cube[B][2][i];
            for (int i=0;i<3;++i) cube[B][2][i] = cube[L][2][i];
            for (int i=0;i<3;++i) cube[L][2][i] = temp[i];
        }
    } else if (f == F) {
        if (dir==1) rotateFaceCW(cube[F]); else rotateFaceCCW(cube[F]);
        // affected: U bottom row, R left col, D top row, L right col
        if (dir==1) {
            // save U bottom
            for (int i=0;i<3;++i) temp[i] = cube[U][2][i];
            // U bottom <- L right (inverted)
            for (int i=0;i<3;++i) cube[U][2][i] = cube[L][2-i][2];
            // L right <- D top
            for (int i=0;i<3;++i) cube[L][i][2] = cube[D][0][i];
            // D top <- R left (inverted)
            for (int i=0;i<3;++i) cube[D][0][i] = cube[R][2-i][0];
            // R left <- saved U bottom
            for (int i=0;i<3;++i) cube[R][i][0] = temp[i];
        } else {
            for (int i=0;i<3;++i) temp[i] = cube[U][2][i];
            for (int i=0;i<3;++i) cube[U][2][i] = cube[R][i][0];
            for (int i=0;i<3;++i) cube[R][i][0] = cube[D][0][2-i];
            for (int i=0;i<3;++i) cube[D][0][i] = cube[L][i][2];
            for (int i=0;i<3;++i) cube[L][i][2] = temp[2-i];
        }
    } else if (f == B) {
        if (dir==1) rotateFaceCW(cube[B]); else rotateFaceCCW(cube[B]);
        // affected: U top row, L left col, D bottom row, R right col
        if (dir==1) {
            for (int i=0;i<3;++i) temp[i] = cube[U][0][i];
            for (int i=0;i<3;++i) cube[U][0][i] = cube[R][i][2];
            for (int i=0;i<3;++i) cube[R][i][2] = cube[D][2][2-i];
            for (int i=0;i<3;++i) cube[D][2][i] = cube[L][i][0];
            for (int i=0;i<3;++i) cube[L][i][0] = temp[2-i];
        } else {
            for (int i=0;i<3;++i) temp[i] = cube[U][0][i];
            for (int i=0;i<3;++i) cube[U][0][i] = cube[L][2-i][0];
            for (int i=0;i<3;++i) cube[L][i][0] = cube[D][2][i];
            for (int i=0;i<3;++i) cube[D][2][i] = cube[R][2-i][2];
            for (int i=0;i<3;++i) cube[R][i][2] = temp[i];
        }
    } else if (f == R) {
        if (dir==1) rotateFaceCW(cube[R]); else rotateFaceCCW(cube[R]);
        // affected: U right col, B left col, D right col, F right col
        if (dir==1) {
            for (int i=0;i<3;++i) temp[i] = cube[U][i][2];
            for (int i=0;i<3;++i) cube[U][i][2] = cube[F][i][2];
            for (int i=0;i<3;++i) cube[F][i][2] = cube[D][i][2];
            for (int i=0;i<3;++i) cube[D][i][2] = cube[B][2-i][0];
            for (int i=0;i<3;++i) cube[B][i][0] = temp[2-i];
        } else {
            for (int i=0;i<3;++i) temp[i] = cube[U][i][2];
            for (int i=0;i<3;++i) cube[U][i][2] = cube[B][2-i][0];
            for (int i=0;i<3;++i) cube[B][i][0] = cube[D][2-i][2];
            for (int i=0;i<3;++i) cube[D][i][2] = cube[F][i][2];
            for (int i=0;i<3;++i) cube[F][i][2] = temp[i];
        }
    } else if (f == L) {
        if (dir==1) rotateFaceCW(cube[L]); else rotateFaceCCW(cube[L]);
        // affected: U left col, F left col, D left col, B right col
        if (dir==1) {
            for (int i=0;i<3;++i) temp[i] = cube[U][i][0];
            for (int i=0;i<3;++i) cube[U][i][0] = cube[B][2-i][2];
            for (int i=0;i<3;++i) cube[B][i][2] = cube[D][2-i][0];
            for (int i=0;i<3;++i) cube[D][i][0] = cube[F][i][0];
            for (int i=0;i<3;++i) cube[F][i][0] = temp[i];
        } else {
            for (int i=0;i<3;++i) temp[i] = cube[U][i][0];
            for (int i=0;i<3;++i) cube[U][i][0] = cube[F][i][0];
            for (int i=0;i<3;++i) cube[F][i][0] = cube[D][i][0];
            for (int i=0;i<3;++i) cube[D][i][0] = cube[B][2-i][2];
            for (int i=0;i<3;++i) cube[B][i][2] = temp[2-i];
        }
    }
}

// Start an animated move (if none in progress)
void startMoveAnimated(int f, int dir) {
    if (animating) return;
    animFace = f;
    animDir = dir;
    animAngle = 0.0f;
    animating = true;
    // push to history (store logical direction for undo)
    moveHistory.push_back(Move(f, dir));
}

// Perform the animation increment and finalize when >= 90 deg
void updateAnimation() {
    if (!animating) return;
    animAngle += animSpeed;
    if (animAngle >= 90.0f - 1e-3f) {
        // apply final logical move
        applyMoveToState(animFace, animDir);
        animating = false;
        animFace = -1;
        animAngle = 0.0f;
    }
    glutPostRedisplay();
}

// Utility to draw a single square sticker given four world coordinates
void drawQuadByCoords(const GLfloat v0[3], const GLfloat v1[3], const GLfloat v2[3], const GLfloat v3[3], const GLfloat col[3]) {
    glColor3fv(col);
    glBegin(GL_POLYGON);
      glVertex3fv(v0);
      glVertex3fv(v1);
      glVertex3fv(v2);
      glVertex3fv(v3);
    glEnd();
    // border
    glColor3f(0,0,0);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
      glVertex3fv(v0);
      glVertex3fv(v1);
      glVertex3fv(v2);
      glVertex3fv(v3);
    glEnd();
}

// For simplicity we render the cube as 27 little cubes (cubies).
// For each cubie at coordinates (-1,0,1) * spacing, we render its exposed faces
// taking color from the cube[face][row][col] arrays.
void renderCube(bool withAnimation) {
    const float spacing = 1.05f;   // distance between cubie centers
    const float half = 0.48f;      // half size of each small cubelet
    // We'll iterate over cubie positions x,y,z in -1,0,1
    // and determine which face-sticker maps map to that cubie.
    for (int y = 1; y >= -1; --y) {          // top to bottom (Y)
        for (int z = -1; z <= 1; ++z) {      // back to front (Z)
            for (int x = -1; x <= 1; ++x) {  // left to right (X)
                // compute base center world coords
                GLfloat cx = x * spacing;
                GLfloat cy = y * spacing;
                GLfloat cz = z * spacing;

                // determine whether this cubie belongs to the rotating layer
                bool inRotLayer = false;
                if (animating) {
                    switch(animFace) {
                        case U: if (y==1) inRotLayer = true; break;
                        case D: if (y==-1) inRotLayer = true; break;
                        case F: if (z==1) inRotLayer = true; break;
                        case B: if (z==-1) inRotLayer = true; break;
                        case R: if (x==1) inRotLayer = true; break;
                        case L: if (x==-1) inRotLayer = true; break;
                    }
                }

                // If this cubie is in rotation layer, wrap rendering of its faces inside a transform
                if (inRotLayer) {
                    glPushMatrix();
                    // translate to rotation center, apply rotation, translate back
                    GLfloat cxent = 0, cyent = 0, czent = 0;
                    // rotation center depends on face: U -> y=+spacing; etc
                    switch(animFace) {
                        case U: cyent = spacing; break;
                        case D: cyent = -spacing; break;
                        case F: czent = spacing; break;
                        case B: czent = -spacing; break;
                        case R: cxent = spacing; break;
                        case L: cxent = -spacing; break;
                    }
                    // Translate to center-of-rotation
                    glTranslatef(cxent, cyent, czent);
                    // rotate appropriate axis
                    if (animFace == U || animFace == D) {
                        glRotatef(animDir * animAngle, 0.0f, 1.0f, 0.0f);
                    } else if (animFace == F || animFace == B) {
                        glRotatef(animDir * animAngle, 0.0f, 0.0f, 1.0f);
                    } else { // R or L
                        glRotatef(animDir * animAngle, 1.0f, 0.0f, 0.0f);
                    }
                    // Translate back
                    glTranslatef(-cxent, -cyent, -czent);
                }

                // Draw small cube faces (only visible faces)
                // Each face of the small cube corresponds to one sticker if it's on the outside.
                // For outer face, pick color from cube face arrays:
                // Map (x,y,z) to face indices (row,col):
                // U face (y==1): row = 0..2 = (1 - z), col = (x + 1)
                // D face (y==-1): row = 0..2 = (z + 1), col = (x + 1)   [must match orientation]
                // F face (z==1): row = 0..2 = (1 - y), col = (x + 1)
                // B face (z==-1): row = 0..2 = (1 - y), col = (1 - x)
                // R face (x==1): row = 0..2 = (1 - y), col = (1 - z)
                // L face (x==-1): row = 0..2 = (1 - y), col = (z + 1)
                // We'll calculate sticker corners in world coordinates for each outer face

                // helper to draw face rectangle at plane orientation
                auto drawFaceIfOuter = [&](int faceIndex, bool isOuter, const GLfloat normal[3]) {
                    if (!isOuter) return;
                    // determine row,col for this cubie on that face
                    int row=0, col=0;
                    switch(faceIndex) {
                        case U: row = 1 - (z + 1 - 1) /*placeholder*/; break;
                    }
                };

                // We'll compute sticker quads explicitly per face:
                // top (U)
                if (y == 1) {
                    int row = 1 - (z + 1 - 0); // adjust below properly
                }

                // Simpler: compute quad corners directly using center and orientation vectors:
                // For each possible outer face (+X, -X, +Y, -Y, +Z, -Z), compute corners and pick color.

                // face +X (right)
                if (x == 1) {
                    // center of face:
                    GLfloat cx_face = cx + half;
                    GLfloat cy_face = cy;
                    GLfloat cz_face = cz;
                    // u axis: along z
                    GLfloat ux = 0.0f, uy = 0.0f, uz = -1.0f;
                    // v axis: along y
                    GLfloat vx = 0.0f, vy = 1.0f, vz = 0.0f;
                    float s = half*1.8f; // sticker size
                    GLfloat p0[3] = { cx_face + ( -s/2 )*ux + ( -s/2 )*vx,
                                      cy_face + ( -s/2 )*uy + ( -s/2 )*vy,
                                      cz_face + ( -s/2 )*uz + ( -s/2 )*vz };
                    GLfloat p1[3] = { cx_face + (  s/2 )*ux + ( -s/2 )*vx,
                                      cy_face + (  s/2 )*uy + ( -s/2 )*vy,
                                      cz_face + (  s/2 )*uz + ( -s/2 )*vz };
                    GLfloat p2[3] = { cx_face + (  s/2 )*ux + (  s/2 )*vx,
                                      cy_face + (  s/2 )*uy + (  s/2 )*vy,
                                      cz_face + (  s/2 )*uz + (  s/2 )*vz };
                    GLfloat p3[3] = { cx_face + ( -s/2 )*ux + (  s/2 )*vx,
                                      cy_face + ( -s/2 )*uy + (  s/2 )*vy,
                                      cz_face + ( -s/2 )*uz + (  s/2 )*vz };
                    // pick color from R face array: row = 1 - y, col = 1 - z
                    int row = 1 - (y + 1) + 1; // simpler compute:
                    // Correct mapping:
                    // For R face: row = 1 - y; col = 1 - z
                    int r = 1 - (y + 1 - 1);
                    int rr = 1 - y;
                    int cc = 1 - z;
                    rr = 1 - y; cc = 1 - z;
                    rr = clamp(rr,0,2); cc = clamp(cc,0,2);
                    int colorIdx = cube[R][rr][cc];
                    drawQuadByCoords(p0,p1,p2,p3, FACE_COLORS[colorIdx]);
                }

                // face -X (left)
                if (x == -1) {
                    GLfloat cx_face = cx - half;
                    GLfloat cy_face = cy;
                    GLfloat cz_face = cz;
                    GLfloat ux = 0.0f, uy = 0.0f, uz = 1.0f;
                    GLfloat vx = 0.0f, vy = 1.0f, vz = 0.0f;
                    float s = half*1.8f;
                    GLfloat p0[3] = { cx_face + ( -s/2 )*ux + ( -s/2 )*vx,
                                      cy_face + ( -s/2 )*uy + ( -s/2 )*vy,
                                      cz_face + ( -s/2 )*uz + ( -s/2 )*vz };
                    GLfloat p1[3] = { cx_face + (  s/2 )*ux + ( -s/2 )*vx,
                                      cy_face + (  s/2 )*uy + ( -s/2 )*vy,
                                      cz_face + (  s/2 )*uz + ( -s/2 )*vz };
                    GLfloat p2[3] = { cx_face + (  s/2 )*ux + (  s/2 )*vx,
                                      cy_face + (  s/2 )*uy + (  s/2 )*vy,
                                      cz_face + (  s/2 )*uz + (  s/2 )*vz };
                    GLfloat p3[3] = { cx_face + ( -s/2 )*ux + (  s/2 )*vx,
                                      cy_face + ( -s/2 )*uy + (  s/2 )*vy,
                                      cz_face + ( -s/2 )*uz + (  s/2 )*vz };
                    // L face mapping: row = 1 - y, col = z + 1
                    int rr = 1 - y;
                    int cc = z + 1;
                    rr = clamp(rr,0,2); cc = clamp(cc,0,2);
                    int colorIdx = cube[L][rr][cc];
                    drawQuadByCoords(p0,p1,p2,p3, FACE_COLORS[colorIdx]);
                }

                // face +Y (top)
                if (y == 1) {
                    GLfloat cx_face = cx;
                    GLfloat cy_face = cy + half;
                    GLfloat cz_face = cz;
                    GLfloat ux = 1.0f, uy = 0.0f, uz = 0.0f;
                    GLfloat vx = 0.0f, vy = 0.0f, vz = -1.0f;
                    float s = half*1.8f;
                    GLfloat p0[3] = { cx_face + ( -s/2 )*ux + ( -s/2 )*vx,
                                      cy_face + ( -s/2 )*uy + ( -s/2 )*vy,
                                      cz_face + ( -s/2 )*uz + ( -s/2 )*vz };
                    GLfloat p1[3] = { cx_face + (  s/2 )*ux + ( -s/2 )*vx,
                                      cy_face + (  s/2 )*uy + ( -s/2 )*vy,
                                      cz_face + (  s/2 )*uz + ( -s/2 )*vz };
                    GLfloat p2[3] = { cx_face + (  s/2 )*ux + (  s/2 )*vx,
                                      cy_face + (  s/2 )*uy + (  s/2 )*vy,
                                      cz_face + (  s/2 )*uz + (  s/2 )*vz };
                    GLfloat p3[3] = { cx_face + ( -s/2 )*ux + (  s/2 )*vx,
                                      cy_face + ( -s/2 )*uy + (  s/2 )*vy,
                                      cz_face + ( -s/2 )*uz + (  s/2 )*vz };
                    // U face mapping: row = 1 - z, col = x + 1
                    int rr = 1 - z;
                    int cc = x + 1;
                    rr = clamp(rr,0,2); cc = clamp(cc,0,2);
                    int colorIdx = cube[U][rr][cc];
                    drawQuadByCoords(p0,p1,p2,p3, FACE_COLORS[colorIdx]);
                }

                // face -Y (bottom)
                if (y == -1) {
                    GLfloat cx_face = cx;
                    GLfloat cy_face = cy - half;
                    GLfloat cz_face = cz;
                    GLfloat ux = 1.0f, uy = 0.0f, uz = 0.0f;
                    GLfloat vx = 0.0f, vy = 0.0f, vz = 1.0f;
                    float s = half*1.8f;
                    GLfloat p0[3] = { cx_face + ( -s/2 )*ux + ( -s/2 )*vx,
                                      cy_face + ( -s/2 )*uy + ( -s/2 )*vy,
                                      cz_face + ( -s/2 )*uz + ( -s/2 )*vz };
                    GLfloat p1[3] = { cx_face + (  s/2 )*ux + ( -s/2 )*vx,
                                      cy_face + (  s/2 )*uy + ( -s/2 )*vy,
                                      cz_face + (  s/2 )*uz + ( -s/2 )*vz };
                    GLfloat p2[3] = { cx_face + (  s/2 )*ux + (  s/2 )*vx,
                                      cy_face + (  s/2 )*uy + (  s/2 )*vy,
                                      cz_face + (  s/2 )*uz + (  s/2 )*vz };
                    GLfloat p3[3] = { cx_face + ( -s/2 )*ux + (  s/2 )*vx,
                                      cy_face + ( -s/2 )*uy + (  s/2 )*vy,
                                      cz_face + ( -s/2 )*uz + (  s/2 )*vz };
                    // D face mapping: row = z + 1, col = x + 1
                    int rr = z + 1;
                    int cc = x + 1;
                    rr = clamp(rr,0,2); cc = clamp(cc,0,2);
                    int colorIdx = cube[D][rr][cc];
                    drawQuadByCoords(p0,p1,p2,p3, FACE_COLORS[colorIdx]);
                }

                // face +Z (front)
                if (z == 1) {
                    GLfloat cx_face = cx;
                    GLfloat cy_face = cy;
                    GLfloat cz_face = cz + half;
                    GLfloat ux = 1.0f, uy = 0.0f, uz = 0.0f;
                    GLfloat vx = 0.0f, vy = 1.0f, vz = 0.0f;
                    float s = half*1.8f;
                    GLfloat p0[3] = { cx_face + ( -s/2 )*ux + ( -s/2 )*vx,
                                      cy_face + ( -s/2 )*uy + ( -s/2 )*vy,
                                      cz_face + ( -s/2 )*uz + ( -s/2 )*vz };
                    GLfloat p1[3] = { cx_face + (  s/2 )*ux + ( -s/2 )*vx,
                                      cy_face + (  s/2 )*uy + ( -s/2 )*vy,
                                      cz_face + (  s/2 )*uz + ( -s/2 )*vz };
                    GLfloat p2[3] = { cx_face + (  s/2 )*ux + (  s/2 )*vx,
                                      cy_face + (  s/2 )*uy + (  s/2 )*vy,
                                      cz_face + (  s/2 )*uz + (  s/2 )*vz };
                    GLfloat p3[3] = { cx_face + ( -s/2 )*ux + (  s/2 )*vx,
                                      cy_face + ( -s/2 )*uy + (  s/2 )*vy,
                                      cz_face + ( -s/2 )*uz + (  s/2 )*vz };
                    // F face mapping: row = 1 - y, col = x + 1
                    int rr = 1 - y;
                    int cc = x + 1;
                    rr = clamp(rr,0,2); cc = clamp(cc,0,2);
                    int colorIdx = cube[F][rr][cc];
                    drawQuadByCoords(p0,p1,p2,p3, FACE_COLORS[colorIdx]);
                }

                // face -Z (back)
                if (z == -1) {
                    GLfloat cx_face = cx;
                    GLfloat cy_face = cy;
                    GLfloat cz_face = cz - half;
                    GLfloat ux = -1.0f, uy = 0.0f, uz = 0.0f;
                    GLfloat vx = 0.0f, vy = 1.0f, vz = 0.0f;
                    float s = half*1.8f;
                    GLfloat p0[3] = { cx_face + ( -s/2 )*ux + ( -s/2 )*vx,
                                      cy_face + ( -s/2 )*uy + ( -s/2 )*vy,
                                      cz_face + ( -s/2 )*uz + ( -s/2 )*vz };
                    GLfloat p1[3] = { cx_face + (  s/2 )*ux + ( -s/2 )*vx,
                                      cy_face + (  s/2 )*uy + ( -s/2 )*vy,
                                      cz_face + (  s/2 )*uz + ( -s/2 )*vz };
                    GLfloat p2[3] = { cx_face + (  s/2 )*ux + (  s/2 )*vx,
                                      cy_face + (  s/2 )*uy + (  s/2 )*vy,
                                      cz_face + (  s/2 )*uz + (  s/2 )*vz };
                    GLfloat p3[3] = { cx_face + ( -s/2 )*ux + (  s/2 )*vx,
                                      cy_face + ( -s/2 )*uy + (  s/2 )*vy,
                                      cz_face + ( -s/2 )*uz + (  s/2 )*vz };
                    // B face mapping: row = 1 - y, col = 1 - x
                    int rr = 1 - y;
                    int cc = 1 - x;
                    rr = clamp(rr,0,2); cc = clamp(cc,0,2);
                    int colorIdx = cube[B][rr][cc];
                    drawQuadByCoords(p0,p1,p2,p3, FACE_COLORS[colorIdx]);
                }

                if (inRotLayer) {
                    glPopMatrix();
                }
            }
        }
    }
}

// OpenGL display callback
void display() {
    glClearColor(0.06f, 0.06f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();

    // simple camera transform
    glTranslatef(0.0f, 0.0f, -10.0f);
    glRotatef(viewPitch, 1.0f, 0.0f, 0.0f);
    glRotatef(viewYaw, 0.0f, 1.0f, 0.0f);

    // light simple shading (not needed but helps)
    GLfloat mat_specular[] = {0.3f, 0.3f, 0.3f, 1.0f};
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, 10.0f);

    // Render cube using current cube[] state (and possibly animation)
    renderCube(true);

    // display instructions text (orthographic overlay)
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    int w = glutGet(GLUT_WINDOW_WIDTH), h = glutGet(GLUT_WINDOW_HEIGHT);
    glOrtho(0, w, 0, h, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1,1,1);
    void *font = GLUT_BITMAP_HELVETICA_12;
    auto drawText = [&](int x, int y, const char *s){
        glRasterPos2i(x, y);
        for (const char *p=s; *p; ++p) glutBitmapCharacter(font, *p);
    };
    drawText(10, h-20, "Controls: a/q U, s/w R, d/e F, f/r L, g/t B, h/y D  | Arrow keys map to faces U/L/R/D");
    drawText(10, h-36, "Numbers 1/2/4/5/6/8/9 rotate view. m/n speed up/down. o = undo last move. ESC exit.");
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}

// idle callback used for animation
void idleFunc() {
    if (animating) updateAnimation();
}

// Map simple character keys to moves
void startKeyMove(unsigned char key) {
    if (animating) return; // ignore new moves while animating
    switch (key) {
        case 'a': startMoveAnimated(U, +1); break;
        case 'q': startMoveAnimated(U, -1); break;
        case 's': startMoveAnimated(R, +1); break;
        case 'w': startMoveAnimated(R, -1); break;
        case 'd': startMoveAnimated(F, +1); break;
        case 'e': startMoveAnimated(F, -1); break;
        case 'f': startMoveAnimated(L, +1); break;
        case 'r': startMoveAnimated(L, -1); break;
        case 'g': startMoveAnimated(B, +1); break;
        case 't': startMoveAnimated(B, -1); break;
        case 'h': startMoveAnimated(D, +1); break;
        case 'y': startMoveAnimated(D, -1); break;
        case 'm': animSpeed = clamp(animSpeed + 1.0f, 1.0f, 30.0f); break;
        case 'n': animSpeed = clamp(animSpeed - 1.0f, 1.0f, 30.0f); break;
        case 'o': // undo last move
            if (!animating && !moveHistory.empty()) {
                Move last = moveHistory.back();
                moveHistory.pop_back();
                // to undo, start the inverse move
                startMoveAnimated(last.face, -last.dir);
            }
            break;
        case '1': viewPitch += 5.0f; break;
        case '2': viewPitch -= 5.0f; break;
        case '4': viewYaw -= 5.0f; break;
        case '6': viewYaw += 5.0f; break;
        case '5': viewPitch = 20.0f; viewYaw = -30.0f; break;
        case '8': viewPitch += 10.0f; break;
        case '9': viewYaw += 45.0f; break;
        case 27: exit(0); break; // ESC
    }
    glutPostRedisplay();
}

// Special keys (arrows)
void specialKey(int key, int, int) {
    if (animating) return;
    switch(key) {
        case GLUT_KEY_UP:    startMoveAnimated(U, +1); break;
        case GLUT_KEY_DOWN:  startMoveAnimated(D, +1); break;
        case GLUT_KEY_LEFT:  startMoveAnimated(L, +1); break;
        case GLUT_KEY_RIGHT: startMoveAnimated(R, +1); break;
        default: break;
    }
    glutPostRedisplay();
}

// standard keyboard callback
void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    startKeyMove(key);
}

// mouse callbacks for drag-to-rotate camera
void mouseButton(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            mouseDown = true;
            lastMouseX = x;
            lastMouseY = y;
        } else {
            mouseDown = false;
        }
    }
}

void mouseMotion(int x, int y) {
    if (!mouseDown) return;
    int dx = x - lastMouseX;
    int dy = y - lastMouseY;
    viewYaw += dx * 0.3f;
    viewPitch += dy * 0.3f;
    viewPitch = clamp(viewPitch, -89.0f, 89.0f);
    lastMouseX = x;
    lastMouseY = y;
    glutPostRedisplay();
}

// window reshape
void reshape(int w, int h) {
    if (h == 0) h = 1;
    float aspect = (float)w / (float)h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, aspect, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

// simple GL init
void initGL() {
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_CULL_FACE);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    GLfloat lightpos[] = { 5.0f, 5.0f, 10.0f, 1.0f };
    GLfloat lightcol[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightcol);
}

// main
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(820, 720);
    glutCreateWindow("Rubik's Cube - fixed");

    initCube();
    initGL();

    glutDisplayFunc(display);
    glutIdleFunc(idleFunc);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKey);
    glutMouseFunc(mouseButton);
    glutMotionFunc(mouseMotion);
    glutReshapeFunc(reshape);

    printf("Rubik's Cube simulator starting.\n");
    printf("Keys: a/q U, s/w R, d/e F, f/r L, g/t B, h/y D\n");
    printf("Arrow keys: Up/Down/Left/Right -> U/D/L/R (clockwise)\n");
    printf("m/n: speed up/down, o: undo last move\n");

    glutMainLoop();
    return 0;
}