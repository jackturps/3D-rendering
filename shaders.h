//
// Created by Jack Turpitt on 10/04/23.
//

#ifndef INC_3D_SHADERS_H
#define INC_3D_SHADERS_H

/**
 * Convert the vertex position into clip space so that it can be UV mapped later.
 */
const char* vertexShaderSource =
        "#version 120\n"
        "attribute vec4 aPos;\n"
        "attribute vec2 aTexCoord;\n"
        "varying vec2 TexCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(aPos.xyz, 1.0);\n"
        "    TexCoord = aTexCoord;\n"
        "}\0";

/**
 * Figure out what colour a pixel should be based on its position in the texture.
 */
const char* fragmentShaderSource =
        "#version 120\n"
        "varying vec2 TexCoord;\n"
        "uniform sampler2D textureSampler;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = texture2D(textureSampler, TexCoord);\n"
        "}\0";

#endif //INC_3D_SHADERS_H
