#pragma once

#include <GL\glew.h>
#include <GL\glut.h>
#define NOMINMAX
#include <windows.h>
#include "SOIL.h"

#define ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200
#include "CL/cl2.hpp"
#include "user_interaction.h"

//#define GL_SHARING_EXTENSION "cl_khr_gl_sharing"   // OpenCL-OpenGL interoperability extension

const int window_width = 1280; // 1280
const int window_height = 720; // 720
							   // OpenGL vertex buffer object
GLuint window_texture;
GLuint albedo_texture;
GLuint normal_texture;
GLuint vbo_albedo;
GLuint vbo_normal;
float * pixels;

// dipslay function prototype
void render();

void initGL(int argc, char** argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(window_width, window_height);
	glutCreateWindow("OpenCL SDF");

	glutDisplayFunc(render);
	glutSetCursor(GLUT_CURSOR_NONE);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(specialkeys);
	glutMouseFunc(mouse);
	glutPassiveMotionFunc(motion);

	glewInit();

	// initialise OpenGL
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glMatrixMode(GL_PROJECTION);
	gluOrtho2D(0.0, window_width, 0.0, window_height);
}

void createTexture(GLuint* texture) {
	glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D, *texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, window_width, window_height, 0, GL_RGBA, GL_FLOAT, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFinish();
}

void loadTexture(char * image_location, GLuint * texture, GLuint * vbo) {	
	glGenTextures(1, texture);
	*texture = SOIL_load_OGL_texture
	(
		image_location,
		SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID,
		SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y | SOIL_FLAG_NTSC_SAFE_RGB | SOIL_FLAG_COMPRESS_TO_DXT
	);
	glBindTexture(GL_TEXTURE_2D, *texture);
	GLfloat size = (1024 * 1024 * 16); // why 16 ?
	GLfloat * data = new float[size];
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, data);
	glBindTexture(GL_TEXTURE_2D, 0);
	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	//glFinish();
}

void createVertexBuffer(GLuint * vbo) {
	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	unsigned int size = window_width * window_height * sizeof(cl_float3);
	glBufferData(GL_ARRAY_BUFFER, size, 0, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void drawGL(int sample_quality) {
	//clear all pixels, then render from the vbo
	glClear(GL_COLOR_BUFFER_BIT);	
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, window_texture);
	glBegin(GL_TRIANGLES);
	glTexCoord2f(-1.0f, 0.0f); glVertex2f(-window_width, 0.0f);
	glTexCoord2f(1.0f, 0.0f); glVertex2f(window_width, 0.0f);
	glTexCoord2f(1.0f, 2.0f); glVertex2f(window_width, window_height * 2.0f);
	//glTexCoord2f(1.0f, 1.0f); glVertex2f(window_width, window_height);
	//glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, window_height);
	//glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
	glEnd();

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisable(GL_TEXTURE_2D);

	// flip backbuffer to screen
	glutSwapBuffers();
}

void Timer(int value) {
	glutPostRedisplay();
	glutTimerFunc(8, Timer, 0); // OpenGL fps limit (1000 ms / 60 fps = 15 ms)
}
