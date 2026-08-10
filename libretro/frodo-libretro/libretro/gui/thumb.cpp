/*
	modded for libretro-frodo
*/

#include <stdlib.h>
#include <string.h>

typedef unsigned  int  PIXEL;

static void ScaleLine(PIXEL *Target, PIXEL *Source,
      int SrcWidth, int TgtWidth)
{
   int NumPixels = TgtWidth;
   int IntPart   = SrcWidth / TgtWidth;
   int FractPart = SrcWidth % TgtWidth;
   int E         = 0;

   while (NumPixels-- > 0)
   {
      *Target++  = *Source;
      Source    += IntPart;
      E         += FractPart;
      if (E >= TgtWidth)
      {
         E      -= TgtWidth;
         Source++;
      } /* if */
   } /* while */
}

void ScaleRect(PIXEL *Target, PIXEL *Source, int SrcWidth, int SrcHeight,
      int TgtWidth, int TgtHeight)
{
   int NumPixels     = TgtHeight;
   int IntPart       = (SrcHeight / TgtHeight) * SrcWidth;
   int FractPart     = SrcHeight % TgtHeight;
   int E             = 0;
   PIXEL *PrevSource = NULL;

   while (NumPixels-- > 0)
   {
      if (Source == PrevSource)
         memcpy(Target, Target-TgtWidth, TgtWidth*sizeof(*Target));
      else
      {
         ScaleLine(Target, Source, SrcWidth, TgtWidth);
         PrevSource = Source;
      } /* if */
      Target += TgtWidth;
      Source += IntPart;
      E      += FractPart;
      if (E >= TgtHeight)
      {
         E      -= TgtHeight;
         Source += SrcWidth;
      } /* if */
   } /* while */
}
