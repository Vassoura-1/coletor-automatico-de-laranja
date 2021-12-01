// fonte: https://www.codeproject.com/Articles/825200/An-Implementation-Of-The-Connected-Component-Label
#include "labelling.h"

#define CALL_LabelComponent(x,y,returnLabel) { STACK[SP] = x; STACK[SP+1] = y; STACK[SP+2] = returnLabel; SP += 3; goto START; }
#define RETURN { SP -= 3;                \
                 switch (STACK[SP+2])    \
                 {                       \
                 case 1 : goto RETURN1;  \
                 case 2 : goto RETURN2;  \
                 case 3 : goto RETURN3;  \
                 case 4 : goto RETURN4;  \
                 default: return;        \
                 }                       \
               }
#define X (STACK[SP-3])
#define Y (STACK[SP-2])


void LabelComponent(unsigned char* STACK, unsigned char width, unsigned char height,
                   uint8_t* input, uint8_t * output,
                   uint8_t  labelNo, short * labelCount,
                   unsigned char x, unsigned char y)
{
  STACK[0] = x;
  STACK[1] = y;
  STACK[2] = 0;  /* return - component is labelled */
  uint8_t  SP   = 3;
  unsigned short index; 

START: /* Recursive routine starts here */

  index = X + width*Y;
  if (input[index]!=1) RETURN;   /* This pixel is not part of a component */
  // if (output[index] != 0) RETURN;   /* This pixel has already been labelled  */
  output[index] = labelNo;
  *labelCount = *labelCount + 1;

  if (X > 0) 
    CALL_LabelComponent(X-1, Y, 1);   /* left  pixel */
RETURN1:

  if (X < width-1)
    CALL_LabelComponent(X+1, Y, 2);   /* rigth pixel */
RETURN2:

  if (Y > 0)
    CALL_LabelComponent(X, Y-1, 3);   /* upper pixel */
RETURN3:

  if (Y < height-1)
    CALL_LabelComponent(X, Y+1, 4);   /* lower pixel */
RETURN4:

  RETURN;
}

bool SizeFiltering(unsigned char width, unsigned char height,
                   uint8_t * input, uint8_t * output,
                   uint8_t labelNo, short label_count,
                   unsigned short max_size, unsigned short min_size){
  bool filtered = false;

  // do nothing if component within  size specifications
  if (label_count > min_size && label_count < max_size)
    return filtered;

  filtered = true;
  // remove component otherwise
  short output_len = height*width -1;
  for (unsigned short index = 0; index < output_len; index++)
  {
    if (output[index] == labelNo){
      output[index] = 0;
      input[index] = 0;
    }
  }
  return filtered;
};

uint8_t LabelImage(unsigned char width, unsigned char height, uint8_t * input, uint8_t * output,
                unsigned short max_size, unsigned short min_size)
{
  unsigned char* STACK = (unsigned char*) heap_caps_malloc(3*sizeof(unsigned char)*(width*height + 1), MALLOC_CAP_SPIRAM);
  
  uint8_t  labelNo = 1; // first label must be 2 to make inplace substitution work
  unsigned short  index  = -1;
  for (unsigned char y = 0; y < height; y++)
  {
    for (unsigned char x = 0; x < width; x++)
    {
      index++;
      if (!input[index]) continue;   /* This pixel is not part of a component */
      // if (output[index] != 0) continue;   /* This pixel has already been labelled  */
      if (labelNo == 255) break; /*safeguard against component number overflow */
      /* New component found */
      labelNo++;
      
      short label_count = 0;
      LabelComponent(STACK, width, height, input, output, labelNo, &label_count, x, y);
      // filter component based on size
      if(SizeFiltering(width, height, input, output, labelNo, label_count, max_size, min_size))
        labelNo--; //
    }
    if (labelNo == 255) break; /*safeguard against component number overflow */
  }

  heap_caps_free(STACK);
  return labelNo;
}