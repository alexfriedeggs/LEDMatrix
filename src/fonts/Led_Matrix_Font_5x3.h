const uint8_t Led_Matrix_Font_5x3Bitmaps[] PROGMEM = {
  0xCC, 0x88, 0x99, 0x80, 0x74, 0x63, 0x17, 0x00, 0x4C, 0x44, 0xE0, 0xF0, 
  0x5D, 0x0F, 0x80, 0xF8, 0x5E, 0x1F, 0x80, 0x94, 0xBE, 0x21, 0x00, 0xFC, 
  0x3C, 0x1F, 0x80, 0x74, 0x3D, 0x17, 0x00, 0xF8, 0x44, 0x44, 0x00, 0x74, 
  0x5D, 0x17, 0x00, 0x74, 0x5E, 0x17, 0x00
};

const GFXglyph Led_Matrix_Font_5x3Glyphs[] PROGMEM = {
  {     0,   5,   5,   6,    0,   -5 },   // 0x2F '/'
  {     4,   5,   5,   6,    0,   -5 },   // 0x30 '0'
  {     8,   4,   5,   4,    0,   -5 },   // 0x31 '1'
  {    11,   5,   5,   6,    0,   -5 },   // 0x32 '2'
  {    15,   5,   5,   6,    0,   -5 },   // 0x33 '3'
  {    19,   5,   5,   6,    0,   -5 },   // 0x34 '4'
  {    23,   5,   5,   6,    0,   -5 },   // 0x35 '5'
  {    27,   5,   5,   6,    0,   -5 },   // 0x36 '6'
  {    31,   5,   5,   6,    0,   -5 },   // 0x37 '7'
  {    35,   5,   5,   6,    0,   -5 },   // 0x38 '8'
  {    39,   5,   5,   6,    0,   -5 }    // 0x39 '9'
};

const GFXfont Led_Matrix_Font_5x3 PROGMEM = {
  (uint8_t  *)Led_Matrix_Font_5x3Bitmaps,   
  (GFXglyph *)Led_Matrix_Font_5x3Glyphs, 0x2F, 0x39,    20 };

// Approx. 1485 bytes
