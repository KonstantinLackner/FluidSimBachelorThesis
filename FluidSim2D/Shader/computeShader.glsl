#version 430

#define N 10
#define NUM_ITERATIONS 20

/* 
There are some memory barriers in here, set so operations that need to be finished before others(e.g.borders and wall cells)
Most of this, however, runs in parallel. Also meaning that the diffuse steps could be a little messed up but other implementations
don't seem to care about this either.
*/

layout(local_size_x = 1, local_size_y = 1) in;
layout(rg32f, binding = 0) uniform image2D previousVectorImage;
layout(rg32f, binding = 1) uniform image2D currentVectorImage;
uniform float viscosity;
uniform float dt;

// Loads the values of the cells around the current cell
void loadAdjacentVectors(layout(rg32f) image2D image, ivec2 coords, out vec2 top, out vec2 left, out vec2 bottom, out vec2 right)
{
	top    = imageLoad(image, coords + ivec2(0,  1)).xy;
	left   = imageLoad(image, coords + ivec2(-1, 0)).xy;
	bottom = imageLoad(image, coords + ivec2(0, -1)).xy;
	right  = imageLoad(image, coords + ivec2(1,  0)).xy;
}

// Slightly different from the other setBounds - This is to avoid the weird int switch
void setBoundsDiffuseAdvect(layout(rg32f) image2D image, ivec2 coords, vec2 newVector) {
    if (coords.x == 1)
	{
		imageStore(image, ivec2(0, coords.y), vec4(-newVector.x, newVector.y, 0, 0));
	}
	else if (coords.x == N)
	{
		imageStore(image, ivec2(N + 1, coords.y), vec4(-newVector.x, newVector.y, 0, 0));
	}

	if (coords.y == 1)
	{
		imageStore(image, ivec2(coords.x, 0), vec4(newVector.x, -newVector.y, 0, 0));
	}
	else if (coords.y == N + 1)
	{
		imageStore(image, ivec2(coords.x, N + 1), vec4(newVector.x, -newVector.y, 0, 0));
	}

	barrier();
	if (coords == ivec2(1, 1))
	{
		imageStore(image, ivec2(0,         0), vec4(0.5 * (imageLoad(image, ivec2(1,     0)).xy + imageLoad(image, ivec2(0,     1)).xy), 0, 0));
		imageStore(image, ivec2(0,     N + 1), vec4(0.5 * (imageLoad(image, ivec2(1, N + 1)).xy + imageLoad(image, ivec2(0,     N)).xy), 0, 0));
		imageStore(image, ivec2(N + 1,     0), vec4(0.5 * (imageLoad(image, ivec2(N,     0)).xy + imageLoad(image, ivec2(N + 1, 1)).xy), 0, 0));
		imageStore(image, ivec2(N + 1, N + 1), vec4(0.5 * (imageLoad(image, ivec2(N, N + 1)).xy + imageLoad(image, ivec2(N + 1, N)).xy), 0, 0));
	}
	barrier();
}

// Second setBounds version
void setBoundsProject(layout(rg32f) image2D image, ivec2 coords, vec2 newVector) {
	if (coords.x == 1)
	{
		imageStore(image, ivec2(0, coords.y), vec4(newVector, 0, 0));
	}
	else if (coords.x == N)
	{
		imageStore(image, ivec2(N + 1, coords.y), vec4(newVector, 0, 0));
	}

	if (coords.y == 1)
	{
		imageStore(image, ivec2(coords.x, 0), vec4(newVector, 0, 0));
	}
	else if (coords.y == N + 1)
	{
		imageStore(image, ivec2(coords.x, N + 1), vec4(newVector, 0, 0));
	}

	barrier();
	if (coords == ivec2(1, 1))
	{
		imageStore(image, ivec2(0,         0), vec4(0.5 * (imageLoad(image, ivec2(1,     0)).xy + imageLoad(image, ivec2(0,     1)).xy), 0, 0));
		imageStore(image, ivec2(0,     N + 1), vec4(0.5 * (imageLoad(image, ivec2(1, N + 1)).xy + imageLoad(image, ivec2(0,     N)).xy), 0, 0));
		imageStore(image, ivec2(N + 1,     0), vec4(0.5 * (imageLoad(image, ivec2(N,     0)).xy + imageLoad(image, ivec2(N + 1, 1)).xy), 0, 0));
		imageStore(image, ivec2(N + 1, N + 1), vec4(0.5 * (imageLoad(image, ivec2(N, N + 1)).xy + imageLoad(image, ivec2(N + 1, N)).xy), 0, 0));
	}
	barrier();
}

void diffuse(ivec2 coords) {
	// Old vector value
	vec2 previousVector = imageLoad(previousVectorImage, coords).xy;

	// Get surrounding current vectors
	vec2 currentVectorTop = imageLoad(currentVectorImage, coords + ivec2(0, 1)).xy;
	vec2 currentVectorRight = imageLoad(currentVectorImage, coords + ivec2(1, 0)).xy;
	vec2 currentVectorBottom = imageLoad(currentVectorImage, coords + ivec2(0, -1)).xy;
	vec2 currentVectorLeft = imageLoad(currentVectorImage, coords + ivec2(-1, 0)).xy;

    float a = dt * viscosity * N * N;

    for (int k = 0; k < NUM_ITERATIONS; k++) {
        vec2 newVector = previousVector + a * (currentVectorLeft + 
            currentVectorRight + currentVectorBottom + currentVectorTop) / (1 + 4 * a);
		imageStore(currentVectorImage, coords, vec4(newVector, 0, 0));
        setBoundsDiffuseAdvect(currentVectorImage, coords, newVector);
    }
}

void project(layout(rg32f) image2D previous, layout(rg32f) image2D current, ivec2 coords) {
	float h = 1.f / N;

	// Get surrounding current vectors
	vec2 currentVectorTop;
	vec2 currentVectorLeft;
	vec2 currentVectorBottom;
	vec2 currentVectorRight;
	loadAdjacentVectors(current, coords, currentVectorTop, currentVectorLeft, currentVectorBottom, currentVectorRight);

	vec2 newVector = vec2(0, -0.5f * h * (currentVectorRight - currentVectorLeft + currentVectorTop - currentVectorBottom).y);
	imageStore(previous, coords, vec4(newVector, 0, 0));

	setBoundsProject(previous, coords, newVector);
	
	vec2 previousVectorTop;
	vec2 previousVectorLeft;
	vec2 previousVectorBottom;
	vec2 previousVectorRight;
	for (int k = 0; k < NUM_ITERATIONS; k++) {
		loadAdjacentVectors(previous, coords, previousVectorTop, previousVectorLeft, previousVectorBottom, previousVectorRight);
		newVector.y = (imageLoad(previous, coords).y + previousVectorLeft.x + previousVectorRight.x + previousVectorBottom.x + previousVectorTop.x) / 4.f;
		imageStore(previous, coords, vec4(newVector, 0, 0));
		setBoundsProject(previous, coords, newVector);
	}
	
	loadAdjacentVectors(previous, coords, previousVectorTop, previousVectorLeft, previousVectorBottom, previousVectorRight);
	newVector = imageLoad(current, coords).xy;
	newVector -= 0.5 * vec2(previousVectorRight.x - previousVectorLeft.x, previousVectorBottom.x - previousVectorTop.x) / h;

	imageStore(current, coords, vec4(newVector, 0, 0));
	setBoundsDiffuseAdvect(current, coords, newVector);
}

void advect(layout(rg32f) image2D previous, layout(rg32f) image2D current, ivec2 coords) {
	float dt0 = dt * N;
	float x, y;

	vec2 currentVector = imageLoad(currentVectorImage, coords).xy;

	x = coords.x - dt0 * currentVector.x;
	y = coords.y - dt0 * currentVector.y;

	if (x < 0.5) {
		x = 0.5;
	}
	else if (x > N + 0.5) {
		x = N + 0.5;
	}

	int i0 = int(x);
	int i1 = i0 + 1;

	if (y < 0.5) {
		y = 0.5;
	}
	else if (y > N + 0.5) {
		y = N + 0.5;
	}

	int j0 = int(y);
	int j1 = j0 + 1;

	float s1 = x - i0;
	float s0 = 1 - s1;
	float t1 = y - j0;
	float t0 = 1 - t1;

	vec2 newVector = s0 * (t0 * imageLoad(previous, ivec2(i0, j0)).xy +
		t1 * imageLoad(previous, ivec2(i0, j1)).xy) +
		s1 * (t0 * imageLoad(previous, ivec2(i1, j0)).xy +
			t1 * imageLoad(previous, ivec2(i1, j1)).xy);

	imageStore(current, coords, vec4(newVector, 0, 0));
	setBoundsDiffuseAdvect(current, coords, newVector);

	/*
	vec2 newCoords = clamp(coords - dt0 * imageLoad(previous, coords).xy, 0.5, N + 0.5);
	ivec2 i = ivec2(newCoords);
	ivec2 j = i + 1;

	ivec2 s = ivec2(0, newCoords.x - i.x);
	s.x = 1 - s.y;
	ivec2 t = ivec2(0, newCoords.y - j.x);
	t.x = 1 - t.y;

	vec2 newVector = s.x * (t.x * imageLoad(previous, ivec2(i.x, j.x)).xy + t.y * imageLoad(previous, ivec2(i.x, j.y)).xy)
				   + s.y * (t.x * imageLoad(previous, ivec2(i.y, j.x)).xy + t.y * imageLoad(previous, ivec2(i.y, j.y)).xy);

	imageStore(current, coords, vec4(newVector, 0, 0));
	setBoundsDiffuseAdvect(current, coords, newVector);
	*/
}

void main()
{
    // Index of the current grid cell
    ivec2 coords = ivec2(gl_GlobalInvocationID.xy) + ivec2(1);

	diffuse(coords);
	project(previousVectorImage, currentVectorImage, coords);

	advect(currentVectorImage, previousVectorImage, coords);
	project(currentVectorImage, previousVectorImage, coords);
}