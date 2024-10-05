typedef enum {
    UP = 8,
    RIGHT = 4,
    DOWN = 1,
    LEFT = 2,
} Directions;

unsigned int getGradientForCoordinates(unsigned int start, unsigned int end, Directions dir, int x, int y, int width, int height);

