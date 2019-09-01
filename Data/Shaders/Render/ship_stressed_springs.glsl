###VERTEX

#version 120

#define in attribute
#define out varying

// Inputs
in vec2 inShipPointTextureCoordinates;
in vec2 inShipPointPosition;
in float inShipPointPlaneId;

// Outputs        
out vec2 vertexTextureCoords;

// Params
uniform mat4 paramOrthoMatrix;

void main()
{
    vertexTextureCoords = inShipPointTextureCoordinates; 
    gl_Position = paramOrthoMatrix * vec4(inShipPointPosition, inShipPointPlaneId, 1.0);
}

###FRAGMENT

#version 120

#define in varying

// Inputs
in vec2 vertexTextureCoords;

// Input texture
uniform sampler2D sharedSpringTexture;

void main()
{
    gl_FragColor = texture2D(sharedSpringTexture, vertexTextureCoords);
} 
