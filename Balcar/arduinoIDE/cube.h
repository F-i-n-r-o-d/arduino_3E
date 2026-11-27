void drawCubeIMU()
{
    float accX, accY, accZ;
    M5.Imu.getAccelData(&accX, &accY, &accZ);

    float rotX = atan2(-accX, accZ);
    float rotY = atan2(accY, sqrt(accX * accX + accZ * accZ));

    const float size = 40.0f;

    struct Vec3 {
        float x, y, z;
    };

    Vec3 vertices[8] = {
        {-size, -size, -size}, // 0
        { size, -size, -size}, // 1
        { size,  size, -size}, // 2
        {-size,  size, -size}, // 3
        {-size, -size,  size}, // 4
        { size, -size,  size}, // 5
        { size,  size,  size}, // 6
        {-size,  size,  size}  // 7
    };

    const int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };

    int projected[8][2];

    // Projekce bodů
    for (int i = 0; i < 8; i++) {

        float x = vertices[i].x;
        float y = vertices[i].y;
        float z = vertices[i].z;

        // Rotace X
        float y1 = y * cos(rotX) - z * sin(rotX);
        float z1 = y * sin(rotX) + z * cos(rotX);

        // Rotace Y
        float x2 = x * cos(rotY) + z1 * sin(rotY);
        float z2 = -x * sin(rotY) + z1 * cos(rotY);

        // Perspektiva
        float distance = 220.0f;
        float scale = distance / (distance + z2);

        projected[i][0] = (int)(x2 * scale) + 120;
        projected[i][1] = (int)(y1 * scale) + 67;
    }

    M5.Lcd.fillScreen(BLACK);

    // Bílé hrany kostky
    for (int i = 0; i < 12; i++) {

        int a = edges[i][0];
        int b = edges[i][1];

        M5.Lcd.drawLine(
            projected[a][0],
            projected[a][1],
            projected[b][0],
            projected[b][1],
            WHITE
        );
    }

    // RGB osy z jednoho rohu (vertex 0)

    int x0 = projected[0][0];
    int y0 = projected[0][1];

    // Červená osa X
    M5.Lcd.drawLine(
        x0,
        y0,
        projected[1][0],
        projected[1][1],
        RED
    );

    // Zelená osa Y
    M5.Lcd.drawLine(
        x0,
        y0,
        projected[3][0],
        projected[3][1],
        GREEN
    );

    // Modrá osa Z
    M5.Lcd.drawLine(
        x0,
        y0,
        projected[4][0],
        projected[4][1],
        BLUE
    );
}