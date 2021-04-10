enum {
  TouchUp,
  TouchDown,
  TouchMotion
};

void Touch_Draw (void);
void Touch_Event (int type, int finger, float x, float y, float dx, float dy, float pressure);
void Touch_Update (void);
