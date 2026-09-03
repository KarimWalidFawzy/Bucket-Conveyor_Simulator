#ifndef BALL_H
#define BALL_H

typedef enum color_t{red,blue,green,orange,yellow};

class ball
{
private:
    color_t color;
    float diameter;
    // Information about black patch
public:
    ball();
    void draw();
    void roll();
};

#endif // BALL_H
