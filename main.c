#include <OpenGL/gl.h>
#include <GLUT/glut.h>

#include <math.h>
#include <printf.h>
#include <sys/time.h>
#include <memory.h>
#include <stdlib.h>
#include "shaders.h"

// Time since the last frame in seconds.
double time_delta;
double last_frame_time;

typedef struct {
    float x;
    float y;
    float z;
} vec3_t;

typedef struct {
    vec3_t position;

    GLfloat* vertices;
    GLuint* indices;
    GLfloat* colors;

    GLuint texture_id;

    int num_vertices;
    int num_indices;
} object_t;

typedef struct {
    int item_size;

    void* buffer;
    int free_offset;
} allocator_t;

allocator_t new_allocator(int item_size, int max_num_items) {
    allocator_t allocator = {
        .item_size   = item_size,
        .buffer      = aligned_alloc(512, max_num_items * item_size),
        .free_offset = 0
    };
    return allocator;
}

void* acquire_memory(allocator_t* allocator, int num_items) {
    void* acquired_pointer = allocator->buffer + allocator->free_offset * allocator->item_size;
    allocator->free_offset += num_items;
    return acquired_pointer;
}

allocator_t vertex_allocator;
allocator_t index_allocator;
allocator_t color_allocator;
allocator_t texture_allocator;

double get_current_time() {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (double)tp.tv_sec * 1000.0 + (double)tp.tv_nsec / 1000000.0;
}

void update_time_delta() {
    double current_time = get_current_time();
    time_delta = (current_time - last_frame_time) / 1000;
    last_frame_time = current_time;
}

void print_float_buffer(GLfloat* buffer, int num_items) {
    int is_first = 1;
    for(int i = 0; i < num_items; i++) {
        if(!is_first) {
            printf(", ");
        }
        printf("%f", buffer[i]);
        is_first = 0;
    }
    printf("\n");
}

void print_int_buffer(GLuint* buffer, int num_items) {
    int is_first = 1;
    for(int i = 0; i < num_items; i++) {
        if(!is_first) {
            printf(", ");
        }
        printf("%d", buffer[i]);
        is_first = 0;
    }
    printf("\n");
}

void get_texture_data(int texture_id) {
// bind the texture object to the 2D texture target
    glBindTexture(GL_TEXTURE_2D, texture_id);

// retrieve the texture image data
    GLint level = 0; // mipmap level (0 for base level)
    GLenum format = GL_RGBA; // format of the pixel data
    GLenum type = GL_FLOAT; // data type of the pixel data
    GLsizei width = 0; // width of the texture image (will be retrieved)
    GLsizei height = 0; // height of the texture image (will be retrieved)
    GLint imageSize = 0; // size of the image data (will be retrieved)
    void* pixels = NULL; // pointer to the image data (will be retrieved)

// get the width and height of the texture image
    glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &height);

// allocate memory for the image data
    imageSize = height * width * 4;
    pixels = malloc(imageSize * sizeof(float));

// retrieve the texture image data
    glGetTexImage(GL_TEXTURE_2D, level, format, type, pixels);
    print_float_buffer(pixels, imageSize);

// free the image data memory
    free(pixels);
}


object_t create_pyramid(float base_width, float height) {
    object_t triangle = {
        .position     = { .x = 0, .y = 0, .z = 0 },
        .num_vertices = 4,
        .num_indices  = 4,
        .texture_id   = 0,
    };
    GLuint v_start = vertex_allocator.free_offset;

    triangle.vertices = acquire_memory(&vertex_allocator, triangle.num_vertices);
    triangle.indices  = acquire_memory(&index_allocator, triangle.num_indices);
    triangle.colors   = acquire_memory(&color_allocator, triangle.num_vertices);

    /**
     * The first 3 values of each vector define the x, y, and z coordinate.
     * The 4th value is the homogenous coordinate, or w coordinate, and is included
     * so that we can do more types of matrix transforms(translation, etc). Its useful
     * to have a constant value that we can multiply by.
     */
    GLfloat template_vertices[] = {
        0.0f, 0.55f, 0.0f, 1.0f,     // Apex.
        0.43f, -0.4f, 0.24f, 1.0f,    // Front Right.
        -0.43f, -0.4f, 0.24f, 1.0f,   // Front Left.
        0.0f, -0.4f, -0.5f, 1.0f,   // Back.
    };
    memcpy(triangle.vertices, template_vertices, sizeof(template_vertices));

    GLuint template_indices[] = {
        v_start + 0, v_start + 1, v_start + 2,
        v_start + 0, v_start + 2, v_start + 3,
        v_start + 0, v_start + 1, v_start + 3,
        v_start + 1, v_start + 2, v_start + 3,
    };
    memcpy(triangle.indices, template_indices, sizeof(template_indices));

    GLfloat template_colors[] = {
        1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f,
    };
    memcpy(triangle.colors, template_colors, sizeof(template_colors));

    return triangle;
}

object_t create_cube(float side_width) {
    object_t cube = {
            .position     = { .x = 0, .y = 0, .z = 0 },
            .num_vertices = 8,
            .num_indices  = 12,
            .texture_id   = 0,
    };
    GLuint v_start = vertex_allocator.free_offset;

    cube.vertices = acquire_memory(&vertex_allocator, cube.num_vertices);
    cube.indices  = acquire_memory(&index_allocator, cube.num_indices);
    cube.colors   = acquire_memory(&color_allocator, cube.num_vertices);

    /**
     * The first 3 values of each vector define the x, y, and z coordinate.
     * The 4th value is the homogenous coordinate, or w coordinate, and is included
     * so that we can do more types of matrix transforms(translation, etc). Its useful
     * to have a constant value that we can multiply by.
     */
    float len = side_width / 2;
    GLfloat template_vertices[] = {
            -len,  -len, -len, 1.0f,
            len,   -len, -len, 1.0f,
            len,   -len, len, 1.0f,
            -len, -len, len, 1.0f,

            -len, len, -len, 1.0f,
            len,  len, -len, 1.0f,
            len,  len, len,  1.0f,
            -len, len, len,  1.0f,
    };
    memcpy(cube.vertices, template_vertices, sizeof(template_vertices));

    GLuint template_indices[] = {
            v_start + 0, v_start + 1, v_start + 2,
            v_start + 0, v_start + 2, v_start + 3,

            v_start + 1, v_start + 0, v_start + 4,
            v_start + 1, v_start + 4, v_start + 5,

            v_start + 2, v_start + 1, v_start + 5,
            v_start + 2, v_start + 5, v_start + 6,

            v_start + 3, v_start + 2, v_start + 7,
            v_start + 2, v_start + 6, v_start + 7,

            v_start + 3, v_start + 7, v_start + 4,
            v_start + 3, v_start + 4, v_start + 0,

            v_start + 4, v_start + 6, v_start + 5,
            v_start + 4, v_start + 7, v_start + 6,
    };
    memcpy(cube.indices, template_indices, sizeof(template_indices));

    GLfloat template_colors[] = {
            1.0f, 1.0f, 1.0f,
            1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, 1.0f, 0.0f,

            0.0f, 1.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
    };
    memcpy(cube.colors, template_colors, sizeof(template_colors));

    return cube;
}

object_t create_quad(float width, float height) {
    object_t quad = {
            .position     = { .x = 0, .y = 0, .z = 0 },
            .num_vertices = 4,
            .num_indices  = 2,
            .texture_id   = 0,
    };
    GLuint v_start = vertex_allocator.free_offset;

    quad.vertices = acquire_memory(&vertex_allocator, quad.num_vertices);
    quad.indices  = acquire_memory(&index_allocator, quad.num_indices);
    quad.colors   = acquire_memory(&color_allocator, quad.num_vertices);

    GLfloat template_vertices[] = {
        -width / 2,  -height / 2, 0.0f, 1.0f,
        width / 2,   -height / 2, 0.0f, 1.0f,
        -width / 2,  height / 2, 0.0f, 1.0f,
        width / 2,   height / 2, 0.0f, 1.0f,
    };
    memcpy(quad.vertices, template_vertices, sizeof(template_vertices));

    GLuint template_indices[] = {
        0, 2, 1,
        1, 2, 3,
    };
    memcpy(quad.indices, template_indices, sizeof(template_indices));

    GLfloat template_colors[] = {
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
    };
    memcpy(quad.colors, template_colors, sizeof(template_colors));


    GLfloat texture_data[] = {
            1.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 0.0f, 1.0f,
            0.0f, 1.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f, 1.0f,
    };
    glGenTextures(1, &quad.texture_id);

    glBindTexture(GL_TEXTURE_2D, quad.texture_id);

    // Set wrapping properties(clamp just uses the edge pixel if we exceed the edge).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Set the filtering properties for sampling.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int texture_width = 2;
    int texture_height = 2;
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, texture_width, texture_height,
        0, GL_RGBA, GL_FLOAT, texture_data
    );

    // Unbind the texture.
    glBindTexture(GL_TEXTURE_2D, 0);

    return quad;
}

void apply_matrix_transform(GLfloat* vertex_pointer, int num_vertices, float matrix[4][4]) {
    for(int vertex_idx = 0; vertex_idx < num_vertices; vertex_idx++) {
        int idx = vertex_idx * 4;

        float result[4] = { 0, 0, 0, 0 };

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                result[i] += matrix[i][j] * vertex_pointer[idx + j];
            }
        }
        for (int i = 0; i < 4; i++) {
            vertex_pointer[idx + i] = result[i];
        }
    }
}

void get_y_rotation_matrix(float output[4][4], float theta) {
    output[0][0] = cos(theta);  output[0][1] = 0; output[0][2] = sin(theta); output[0][3] = 0;
    output[1][0] = 0;           output[1][1] = 1; output[1][2] = 0;          output[1][3] = 0;
    output[2][0] = -sin(theta); output[2][1] = 0; output[2][2] = cos(theta); output[2][3] = 0;
    // NOTE: We need to set w to 1 here to ensure we don't zero it for future operations.
    output[3][0] = 0;           output[3][1] = 0; output[3][2] = 0;          output[3][3] = 1;
}

void get_x_rotation_matrix(float output[4][4], float theta) {
    output[0][0] = 1; output[0][1] = 0;          output[0][2] = 0;           output[0][3] = 0;
    output[1][0] = 0; output[1][1] = cos(theta); output[1][2] = -sin(theta); output[1][3] = 0;
    output[2][0] = 0; output[2][1] = sin(theta); output[2][2] = cos(theta);  output[2][3] = 0;
    output[3][0] = 0; output[3][1] = 0;          output[3][2] = 0;           output[3][3] = 1;
}

void get_translate_matrix(float output[4][4], float x_distance, float y_distance, float z_distance) {
    output[0][0] = 1; output[0][1] = 0; output[0][2] = 0; output[0][3] = x_distance;
    output[1][0] = 0; output[1][1] = 1; output[1][2] = 0; output[1][3] = y_distance;
    output[2][0] = 0; output[2][1] = 0; output[2][2] = 1; output[2][3] = z_distance;
    output[3][0] = 0; output[3][1] = 0; output[3][2] = 0; output[3][3] = 1;
}

vec3_t add_vectors(vec3_t vec1, vec3_t vec2) {
    vec3_t new_vec = {
        .x = vec1.x + vec2.x,
        .y = vec1.y + vec2.y,
        .z = vec1.z + vec2.z,
    };
    return new_vec;
}

void translate_object(object_t* shape, vec3_t distance) {
    float translation_matrix[4][4];
    get_translate_matrix(translation_matrix, distance.x, distance.y, distance.z);
    apply_matrix_transform(shape->vertices, shape->num_vertices, translation_matrix);

    shape->position = add_vectors(shape->position, distance);
}

void rotate_object(object_t* shape, float rotation_matrix[4][4]) {
    /**
     * All rotations happen around the origin so we need to translate back to the origin before rotating,
     * and translate back to our position after rotating.
     * TODO: Support multiple rotations at the origin without multiple translations in between. Oh actually might not
     * be necessary if rotation matrices can be combined.
     */
    vec3_t position = shape->position;

    float translation_matrix[4][4];
    get_translate_matrix(translation_matrix, -position.x, -position.y, -position.z);
    apply_matrix_transform(shape->vertices, shape->num_vertices, translation_matrix);

    apply_matrix_transform(shape->vertices, shape->num_vertices, rotation_matrix);

    get_translate_matrix(translation_matrix, position.x, position.y, position.z);
    apply_matrix_transform(shape->vertices, shape->num_vertices, translation_matrix);
}


GLuint shaderProgram;
GLuint vertexShader;
GLuint fragmentShader;

object_t pyramid;
object_t cube;
object_t quad;

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    update_time_delta();

    // TODO: Its probably better to apply all transforms to each vertex as we iterate instead of iterating multiple times.
    float transform_matrix[4][4];

//    get_x_rotation_matrix(transform_matrix, 0.63f * time_delta);
//    rotate_object(&cube, transform_matrix);
    get_y_rotation_matrix(transform_matrix, 0.5f * time_delta);
    rotate_object(&quad, transform_matrix);
//
//    get_x_rotation_matrix(transform_matrix, -0.5f * time_delta);
//    rotate_object(&pyramid, transform_matrix);
//    get_y_rotation_matrix(transform_matrix, -0.8f * time_delta);
//    rotate_object(&pyramid, transform_matrix);

    glUseProgram(shaderProgram);

    // This code draws the shapes with a texture.
    // TODO: The built in texture mapping stuff is deprecated, switch to custom shaders.
    GLfloat texture_uvs[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    };

    // Load the shapes texture.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, quad.texture_id);

    // Create a vertex array object that we can use for assigning the vertex attribute arrays.
    GLuint vertex_array_object;
    glGenVertexArraysAPPLE(1, &vertex_array_object);
    glBindVertexArrayAPPLE(vertex_array_object);

    // Setup the buffer for storing vertex data and assign it to the appropriate input in the shader.
    GLuint vbo_vertices;
    glGenBuffers(1, &vbo_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
    glBufferData(GL_ARRAY_BUFFER, quad.num_vertices * 4 * sizeof(GLfloat), quad.vertices, GL_STATIC_DRAW);
    GLint posAttrib = glGetAttribLocation(shaderProgram, "aPos");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 4, GL_FLOAT, GL_FALSE, 0, 0);

    // Do the same but for the texture UVs.
    GLuint vbo_texture_coords;
    glGenBuffers(1, &vbo_texture_coords);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_texture_coords);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texture_uvs), texture_uvs, GL_STATIC_DRAW);
    GLint texAttrib = glGetAttribLocation(shaderProgram, "aTexCoord");
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

    // Set the texture sampler for the shader. Because we're using GL_TEXTURE0 we set this to 0.
    GLint textureLocation = glGetUniformLocation(shaderProgram, "textureSampler");
    glUniform1i(textureLocation, 0);  // Set the value of the uniform variable to 0 (texture unit 0)

    // TODO: Because we're just drawing the quad here all of the indices start from 0, I think in the end these will
    // need to go back to being relative to the entire vertex array.
    glDrawElements(GL_TRIANGLES, quad.num_indices * 3, GL_UNSIGNED_INT, quad.indices);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glutSwapBuffers();
}

void load_shader_program() {
    // Compile the vertex shader.
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // Log and exit if the compilation failed.
    int compile_success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compile_success);
    if (!compile_success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("ERROR COMPILING VERTEX SHADER: %s\n", infoLog);
        exit(1);
    }

    // Do the same for the fragment shader.
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    shaderProgram = glCreateProgram();

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compile_success);
    if (!compile_success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("ERROR COMPILING FRAGMENT SHADER: %s\n", infoLog);
        exit(1);
    }

    // Attach the vertex and fragment shaders to the shader program.
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    // Link and use the shader program.
    glLinkProgram(shaderProgram);

    // Log and exit if there was a problem with the linking.
    int linkStatus;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        printf("LINKING ERROR: %s\n", infoLog);
        exit(1);
    }

    glUseProgram(shaderProgram);
}

int main(int argc, char** argv) {
    last_frame_time = get_current_time();

    glutInit(&argc, argv);
    glutCreateWindow("Jacks 3-Dimensional Wonderland");
    glutDisplayFunc(display);
    glutIdleFunc(display);

    // This enables z-buffering so pixels are occluded based on depth.
    glEnable(GL_DEPTH_TEST);


    const char* version = (const char*)glGetString(GL_VERSION);
    int major, minor;
    sscanf(version, "%d.%d", &major, &minor);
    printf("OpenGL version supported by your graphics card: %s\n", version);

    load_shader_program();

    // TODO: Should really only need a single allocator.
    vertex_allocator = new_allocator(sizeof(GLfloat) * 4, 1024);
    index_allocator = new_allocator(sizeof(GLuint) * 3, 1024);
    color_allocator = new_allocator(sizeof(GLfloat) * 3, 1024);
    texture_allocator = new_allocator(sizeof(GLfloat) * 4, 1024);

//    pyramid1 = create_pyramid(0, 0);
//    pyramid = create_pyramid(0, 0);
//    cube = create_cube(0.5f);
    quad = create_quad(0.8f, 0.4f);

//    vec3_t distance = {.x = -0.5f, .y = -0.5f};
//    translate_object(&pyramid, distance);
//
//    distance.x = 0.5;
//    distance.y = 0.5;
//    translate_object(&cube, distance);

    glutMainLoop();
    return 0;
}

