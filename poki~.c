/**
 @file poki~.c
 poki~ : interpolating, overdubbing poke~ replacement for max/MSP 5
 
 created 11/01/2010 emb
 
 */

#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h"
#include "z_dsp.h"
#include "buffer.h"
#include "ext_atomic.h"

#include "poki_math.h"

void *poki_class;

typedef struct _poki
{
  t_pxobject p_obj;
  t_symbol *p_sym;
  t_buffer *p_buf;
	short p_connected;
	long p_idx0;
	float p_y0;
  double p_preLevel;
  double p_preLevelTarget;
  double p_recLevel;
  double p_recLevelTarget;
  double p_preFadeInc;
  bool p_preFadeFlag;
  double p_recFadeInc;
  bool p_recFadeFlag;
  long p_fadeSamps;
  long p_interpThresholdSamps;
} t_poki;


void *poki_new(t_symbol *s, long thresh);
t_int *poki_perform(t_int *w);
void poki_dsp(t_poki *x, t_signal **sp, short *count);
void poki_set(t_poki *x, t_symbol *s);
void poki_threshold(t_poki *x, long thresh);
void poki_assist(t_poki *x, void *b, long m, long a, char *s);
void poki_dblclick(t_poki *x);

t_max_err poki_preLevel_set(t_poki *x, void *attr, long ac, t_atom *av);
t_max_err poki_preLevel_get(t_poki *x, void *attr, long *ac, t_atom **av);
t_max_err poki_recLevel_set(t_poki *x, void *attr, long ac, t_atom *av);
t_max_err poki_recLevel_get(t_poki *x, void *attr, long *ac, t_atom **av);
t_max_err poki_fadeSamps_set(t_poki *x, void *attr, long ac, t_atom *av);
t_max_err poki_fadeSamps_get(t_poki *x, void *attr, long *ac, t_atom **av);
t_max_err poki_interpThresholdSamps_set(t_poki *x, void *attr, long ac, t_atom *av);
t_max_err poki_interpThresholdSamps_get(t_poki *x, void *attr, long *ac, t_atom **av);

t_symbol *ps_buffer;
int main(void)
{
	t_class *c;

	c = class_new("poki~", (method)poki_new, (method)dsp_free, (short)sizeof(t_poki), 0L, A_SYM, A_DEFLONG, 0);
	class_addmethod(c, (method)poki_dsp, "dsp", A_CANT, 0);
	class_addmethod(c, (method)poki_set, "set", A_SYM, 0);
	class_addmethod(c, (method)poki_assist, "assist", A_CANT, 0);
	class_addmethod(c, (method)poki_dblclick, "dblclick", A_CANT, 0);
  
	CLASS_ATTR_LONG(c, "overdub", 0, t_poki, p_preLevel);
	CLASS_ATTR_SAVE(c, "overdub", 0);
	CLASS_ATTR_ACCESSORS(c, "overdub", (method)poki_preLevel_get, (method)poki_preLevel_set);
  
	CLASS_ATTR_LONG(c, "record", 0, t_poki, p_recLevel);
	CLASS_ATTR_SAVE(c, "record", 0);
	CLASS_ATTR_ACCESSORS(c, "record", (method)poki_recLevel_get, (method)poki_recLevel_set);
  
	CLASS_ATTR_LONG(c, "fade", 0, t_poki, p_fadeSamps);
	CLASS_ATTR_SAVE(c, "fade", 0);
	CLASS_ATTR_ACCESSORS(c, "fade", (method)poki_fadeSamps_get, (method)poki_fadeSamps_set);
  
	CLASS_ATTR_LONG(c, "threshold", 0, t_poki, p_interpThresholdSamps);
	CLASS_ATTR_SAVE(c, "threshold", 0);
	CLASS_ATTR_ACCESSORS(c, "threshold", (method)poki_interpThresholdSamps_get, (method)poki_interpThresholdSamps_set);
  
	class_dspinit(c);
  
	class_register(CLASS_BOX, c);
	poki_class = c;
	ps_buffer = gensym("buffer~");
	
	printf("pokimain\n");

	return 0;
}


t_int *poki_perform(t_int *w)
  {
  t_float * in = (t_float *)(w[1]);
  t_float * out = (t_float *)(w[2]);
  t_poki * x = (t_poki *)(w[3]);
  t_float * index = x->p_connected ? (t_float *)(w[4]) : NULL;
	int n = (int)(w[5]);
	
	if (index == NULL)
    goto out;
	
	t_buffer *b = x->p_buf;
	float *tab;
  //  long idx;
  long frames,nc;
	
	if (x->p_obj.z_disabled)
		goto out;
  
	if (!b)
		goto out;
  
  // buffer data structure exists
	ATOMIC_INCREMENT(&b->b_inuse);
	if (!b->b_valid) {
		ATOMIC_DECREMENT(&b->b_inuse);
		goto out;
	}
  
	tab = b->b_samples;
	frames = b->b_frames;
	nc = b->b_nchans;
	if (nc != 1)
	{
    ATOMIC_DECREMENT(&b->b_inuse);
    goto zero;
	}
	else
	{
		while (n--)
    {
			const long idx = (long)(*index + 0.5f) % frames;
        
			const int step = (idx - x->p_idx0) % frames;
			int interpCount = step-1;
      float input = *in;

			if (x->p_preFadeFlag)
      {
        x->p_preLevel += x->p_preFadeInc;
        if (absDif(x->p_preLevel, x->p_preLevelTarget) < 0.001)
        {
          x->p_preLevel = x->p_preLevelTarget;
          x->p_preFadeFlag = 0;
        }
      }

      if (x->p_recFadeFlag)
      {
        x->p_recLevel += x->p_recFadeInc;
        if (absDif(x->p_recLevel, x->p_recLevelTarget) < 0.001)
        {
          x->p_recLevel = x->p_recLevelTarget;
          x->p_recFadeFlag = 0;
        }
      }
      // these macros are weirdly undefined in deployment builds for some target architectures
      //if (FIX_DENORM_DOUBLE(x->p_recLevel) > 0.00001)
      if (absDif(x->p_recLevel, 0.0) > 1e-6) // -120dB
      { // recording level is non-zero
        input *= x->p_recLevel;
        //if (FIX_DENORM_DOUBLE(x->p_preLevel) > 0.0)
        if (absDif(x->p_preLevel, 0.0) > 1e-6)
        {
          if (absDif(x->p_preLevel, 1.0) < 0.001)
          {
            input += tab[idx];
          }
          else
          { // pre-level is non-unity
            input += (tab[idx] * x->p_preLevel);
          }
        }
      }
      else
      { 
        /*
        // no recording, use overdub level only
        input = 0.0;
        if (absDif(x->p_preLevel, 1.0) < 0.001)
        { // pre level is unity
          input = tab[idx]; // TODO: should just skip this sample
        }
        else
        { // pre level is non-unity
          input = tab[idx] * FIX_DENORM_DOUBLE(x->p_preLevel);
        }
         */
        // with no recording, we don't change the buffer
        input = tab[idx];
        interpCount =  x->p_interpThresholdSamps + 1; // this should cause the interp steps to be skipped
      }
      
      // perform interpolation
			if (interpCount <= x->p_interpThresholdSamps)
			{
        /*
				const float y3 = x->p_y0;
				const float y2 = tab[(x->p_idx0 - step) % frames];
				const float y1 = tab[(idx - (step*2)) % frames];
				const float y0 = tab[(idx - (step*3)) % frames];		
         */
				const float phaseInc = 1.f / ((float)step);
				float phase=phaseInc;
				int interpIdx = (x->p_idx0 + 1) % frames;
				
				while (interpCount > 0)
				{
					// tab[interpIdx] = hermite_interp(phase, y0, y1, y2, y3);
          // not going to attempt higher-order interpolation of input, just yet
					tab[interpIdx] = x->p_y0 + (phase * (input - x->p_y0));
					phase += phaseInc;
					interpIdx = ((interpIdx + 1) % frames);
					interpCount--;
				}
			} // interp count < thresh
      
			//*out = tab[idx];
      
      // output with interpolation 
      const long iIdx = (long)(*index);
      const float fIdx = *index - (long)(*index);
      
      // 3rd-order:
      // *out = hermite_interp(fIdx,
      //                      tab[(iIdx+1) % frames],
      //                      tab[(iIdx+2) % frames], 
      //                      tab[(iIdx+3) % frames],
      //                      tab[(iIdx+4) % frames]
      //                      );
      
      // linear:
      const float tab0 = tab[(iIdx+1) % frames];
      *out =  tab0 + fIdx * (tab[(iIdx + 2) % frames] - tab0);
    
			tab[idx] = input;
			x->p_y0 = input;
      x->p_idx0 = idx;
			out++;
			in++;
			index++;
		} // sample loop
    object_method(b, gensym("dirty"));
    ATOMIC_DECREMENT(&b->b_inuse);
	} // test for mono
	return w + 6;
zero:
  while(n--) *out++ = 0.0;
out:
	return w + 6;
}

void poki_set(t_poki *x, t_symbol *s)
{
	t_buffer *b;
	
	if (s)
  {
		x->p_sym = s;
		if ((b = (t_buffer *)(s->s_thing)) && ob_sym(b) == ps_buffer) {
			x->p_buf = b;
		} else {
			object_error((t_object *)x, "buffer not found", s->s_name);
			x->p_buf = 0;
		}
	} else {
		object_error((t_object *)x, "no buffer argument?");
	}
}

void poki_dsp(t_poki *x, t_signal **sp, short *count)
{
    poki_set(x,x->p_sym);
	x->p_connected = count[1];
    dsp_add(poki_perform, 5, sp[0]->s_vec, sp[2]->s_vec, x, sp[1]->s_vec, sp[0]->s_n);
}

void poki_dblclick(t_poki *x)
{
	t_buffer *b;	
	if ((b = (t_buffer *)(x->p_sym->s_thing)) && ob_sym(b) == ps_buffer)
  {
		mess0((t_object *)b, gensym("dblclick"));
  }
}

void poki_assist(t_poki *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_OUTLET)
		sprintf(s,"(signal) audio output at index (from previous write cycle)");
	else
  {
		switch (a)
    {	
			case 0:	sprintf(s,"(signal) audio input");	break;
			case 1:	sprintf(s,"(signal) write index");	break;
		}
	}
}

t_max_err poki_preLevel_set(t_poki *x, void *attr, long ac, t_atom *av)
{
  if (ac && av)
  {
    const double target = atom_getfloat(av);
    if (absDif(target, x->p_preLevelTarget) > 0.0001)
    {
      x->p_preLevelTarget = target;
      x->p_preFadeInc = (x->p_preLevelTarget - x->p_preLevel) / x->p_fadeSamps;
      x->p_preFadeFlag = 1;
    }
    else
    {
      x->p_preLevelTarget = target;
      x->p_preLevel = target;
      x->p_preFadeInc = 0.0;
      x->p_preFadeFlag = 0;
    }
  }
  return MAX_ERR_NONE;
}

t_max_err poki_preLevel_get(t_poki *x, void *attr, long *ac, t_atom **av)
{
	if (ac && av)
  {
		char alloc;
		if (atom_alloc(ac, av, &alloc))
    {
			return MAX_ERR_GENERIC;
		}
		atom_setfloat(*av, x->p_preLevel);
	}
	return MAX_ERR_NONE;
}


t_max_err poki_recLevel_set(t_poki *x, void *attr, long ac, t_atom *av)
{
  if (ac && av)
  {
    const double target = atom_getfloat(av);
    if (absDif(target, x->p_recLevelTarget) > 0.0001)
    {
      x->p_recLevelTarget = target;
      if (absDif(target, x->p_recLevel) > 0.0001)
      {
        x->p_recFadeInc = (x->p_recLevelTarget - x->p_recLevel) / x->p_fadeSamps;
        x->p_recFadeFlag = 1;
      }
      else
      {
        x->p_recFadeFlag = 0;
      }
    }
    else
    {
      x->p_recLevelTarget = target;
      x->p_recLevel = target;
      x->p_recFadeInc = 0.0;
      x->p_recFadeFlag = 0;
    }
  }
  return MAX_ERR_NONE;
}

t_max_err poki_recLevel_get(t_poki *x, void *attr, long *ac, t_atom **av)
{
	if (ac && av)
  {
		char alloc;
		if (atom_alloc(ac, av, &alloc))
    {
			return MAX_ERR_GENERIC;
		}
		atom_setfloat(*av, x->p_recLevel);
	}
	return MAX_ERR_NONE;
}


t_max_err poki_fadeSamps_set(t_poki *x, void *attr, long ac, t_atom *av)
{
  if (ac && av)
  {
    const long samps = atom_getlong(av);
    if ((samps != x->p_fadeSamps) && (samps > 0))
    {
      x->p_fadeSamps = samps; 
      x->p_preFadeInc = (x->p_preLevelTarget - x->p_preLevel) / x->p_fadeSamps;
      x->p_recFadeInc = (x->p_recLevelTarget - x->p_recLevel) / x->p_fadeSamps;
    }
  }
  return MAX_ERR_NONE;
}

t_max_err poki_fadeSamps_get(t_poki *x, void *attr, long *ac, t_atom **av)
{
	if (ac && av)
  {
		char alloc;	
		if (atom_alloc(ac, av, &alloc))
    {
			return MAX_ERR_GENERIC;
		}
		atom_setlong(*av, x->p_fadeSamps);
	}
	return MAX_ERR_NONE;
}

t_max_err poki_interpThresholdSamps_set(t_poki *x, void *attr, long ac, t_atom *av)
{
  if (ac && av)
  {
    x->p_interpThresholdSamps = atom_getlong(av);
  }
	return MAX_ERR_NONE;
}

t_max_err poki_interpThresholdSamps_get(t_poki *x, void *attr, long *ac, t_atom **av)
{
	if (ac && av)
  {
		char alloc;
		if (atom_alloc(ac, av, &alloc))
    {
			return MAX_ERR_GENERIC;
		}
		atom_setlong(*av, x->p_interpThresholdSamps);
	}
	return MAX_ERR_NONE;
}

void *poki_new(t_symbol *s, long chan)
{
	t_poki *x = object_alloc(poki_class);
	dsp_setup((t_pxobject *)x, 2);	// 2 signal inlets
	outlet_new((t_object *)x, "signal");
	x->p_sym = s;
  x->p_preLevel = 0.0;
  x->p_recLevel = 1.0;
  x->p_fadeSamps = 16;
  x->p_interpThresholdSamps = 16;
  x->p_preFadeInc = 0.0625;
  x->p_recFadeInc = 0.0625;
  x->p_preFadeFlag = 0;
  x->p_recFadeFlag = 0;
	return (x);	
}
