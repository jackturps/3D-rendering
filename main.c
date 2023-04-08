#include <OpenGL/gl.h>
#include <GLUT/glut.h>

#include <math.h>
#include <printf.h>
#include <sys/time.h>
#include <memory.h>
#include <stdlib.h>


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

object_t create_pyramid(float base_width, float height) {
    object_t triangle = {
        .position     = { .x = 0, .y = 0, .z = 0 },
        .num_vertices = 4,
        .num_indices  = 4,
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

object_t pyramid1;
object_t pyramid2;

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


void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    update_time_delta();

    // TODO: Its probably better to apply all transforms to each vertex as we iterate instead of iterating multiple times.
    float transform_matrix[4][4];

    vec3_t move_distance = {
        .x = 0.2f * sin(time_delta / 10),
        .y = 0, // 0.05f * time_delta,
        .z = 0,
    };
    translate_object(&pyramid1, move_distance);

    get_x_rotation_matrix(transform_matrix, 3.13f * time_delta);
    rotate_object(&pyramid1, transform_matrix);
    get_y_rotation_matrix(transform_matrix, 4.7f * time_delta);
    rotate_object(&pyramid1, transform_matrix);

    get_x_rotation_matrix(transform_matrix, -3.0f * time_delta);
    rotate_object(&pyramid2, transform_matrix);
    get_y_rotation_matrix(transform_matrix, -10.0f * time_delta);
    rotate_object(&pyramid2, transform_matrix);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    // The first argument here specifies the size of each vertex/colour, not the number of items in the array.
    glVertexPointer(4, GL_FLOAT, 0, vertex_allocator.buffer);
    glColorPointer(3, GL_FLOAT, 0, color_allocator.buffer);

    // The total number of items is specified HERE.
    glDrawElements(GL_TRIANGLES, index_allocator.free_offset * 3, GL_UNSIGNED_INT, index_allocator.buffer);

    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glutSwapBuffers();
}

int main(int argc, char** argv) {
    last_frame_time = get_current_time();

    vertex_allocator = new_allocator(sizeof(GLfloat) * 4, 1024);
    index_allocator = new_allocator(sizeof(GLuint) * 3, 1024);
    color_allocator = new_allocator(sizeof(GLfloat) * 3, 1024);

    pyramid1 = create_pyramid(0, 0);
    pyramid2 = create_pyramid(0, 0);

    glutInit(&argc, argv);
    glutCreateWindow("Jacks 3-Dimensional Wonderland");
    glutDisplayFunc(display);
    glutIdleFunc(display);

    // This enables z-buffering so pixels are occluded based on depth.
    glEnable(GL_DEPTH_TEST);

    glutMainLoop();
    return 0;
}

