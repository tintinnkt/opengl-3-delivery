#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture_diffuse1;
uniform bool useColor;
uniform vec3 flatColor;

// simple directional light
uniform vec3 lightDir;
uniform vec3 lightColor;

void main()
{
    if (useColor) {
        // flat color mode (used for ground, package, zone marker)
        vec3 norm = normalize(Normal);
        vec3 ld = normalize(-lightDir);
        float diff = max(dot(norm, ld), 0.0);
        vec3 ambient = 0.4 * flatColor;
        vec3 diffuse = diff * lightColor * flatColor;
        FragColor = vec4(ambient + diffuse, 1.0);
    } else {
        // textured mode (used for player, wall models)
        vec4 texColor = texture(texture_diffuse1, TexCoords);
        vec3 norm = normalize(Normal);
        vec3 ld = normalize(-lightDir);
        float diff = max(dot(norm, ld), 0.0);
        vec3 ambient = 0.4 * texColor.rgb;
        vec3 diffuse = diff * lightColor * texColor.rgb;
        FragColor = vec4(ambient + diffuse, texColor.a);
    }
}
