// Copyright (C) 2007-12 Andrea Vedaldi and Brian Fulkerson.
// All rights reserved.

/** @internal
 ** @file   vlfeat_helperlib.cpp
 ** @brief   vl_sift based on (c) vlfeat.org &
 ** http://synaptic-activity.blogspot.de/2012/02/vlfeat-sift-with-opencv-code.html
 ** @author Julian heuser
 **/

/************************************************** Includes *********************************************************/

#include "vlfeat_helperlib.h"

/******************************************** Function Definitions ***************************************************/

void vlfeat_helperlib::vlsift(IplImage* i_image, void* o_DATAdescr, double* o_DATAframes, int* o_nframes, vl_sift_options opts)
{	
	int imHeight, imWidth;
	imHeight = i_image->height;
	imWidth = i_image->width;

    //Take IplImage -> convert to SINGLE (float):
    float* frame = (float*)malloc(imHeight*imWidth*sizeof(float));
    uchar* Ldata = (uchar*)i_image->imageData;
	int ws = i_image->widthStep;
	int chns = i_image->nChannels;


	for(int i=0; i<imHeight; i++)
	{
        for(int j=0; j<imWidth; j++)
		{
            frame[j*imHeight+i*chns] = (float)Ldata[i*ws+j*chns];
		}
	}
 
    // vlsift computation:
    vl_sift_pix const *data;
    int M, N;
    data = (vl_sift_pix*)frame;
    M = i_image->height;
    N = i_image->width;
                                                                                                                       
    // parse vlsift parameters
    int                _verbose				= opts.verbose; // change to 2 for more verbose..
    int                _O					= opts.O; //Octaves
    int                _S					= opts.S; //Levels
    int                _o_min				= opts.o_min;
    double             _edge_thresh			= opts.edge_thresh;  //-1 will use the default (as in matlab)
    double             _peak_thresh			= opts.peak_thresh;
    double             _norm_thresh			= opts.norm_thresh;
    double             _magnif				= opts.magnif;
    double             _window_size			= opts.window_size;
	//double			   *ikeys				= 0;
    int                nikeys				= opts.nikeys; 
    vl_bool            force_orientations	= opts.force_orientations;
    vl_bool            floatDescriptors		= opts.floatDescriptors;
	bool				_ikeys_provided		= opts.ikeys_provided;

	double *ikeys_array = (double*)calloc(4*nikeys,sizeof(double));
	double *ikeys = (double*)calloc(4*nikeys,sizeof(double));
	//check if input keys are sorted
	if(_ikeys_provided)
	{	
		 std::memcpy(ikeys_array, opts.ikeys,4*nikeys*sizeof(double)); // make hard copy
		ikeys = ikeys_array;
		if (!check_sorted(ikeys, nikeys)) 
		{
			printf("vl_sift: Sorting input keys...\n");
			qsort(ikeys, nikeys, 4*sizeof(double), korder);
		}
		printf("vl_sift: Input keys are already sorted!\n");
	}
	else
	{
		free(ikeys_array);
		free(ikeys);
	}



	/* -----------------------------------------------------------------
    *                                                            Do job
    * -------------------------------------------------------------- */


	VlSiftFilt	*filt;
	vl_bool		first;
	double		*frames = 0;
	//vl_uint8	*descr = 0;
	void		*descr = 0;
	int			reserved = 0;
	int			i,j,q;

	/* create a filter to process the image */
	filt = vl_sift_new (M, N, _O, _S, _o_min) ;

	if (_peak_thresh >= 0) 
		vl_sift_set_peak_thresh(filt, _peak_thresh);
	if (_edge_thresh >= 0) 
		vl_sift_set_edge_thresh(filt, _edge_thresh);
	if (_norm_thresh >= 0) 
		vl_sift_set_norm_thresh(filt, _norm_thresh);
	if (_magnif      >= 0) 
		vl_sift_set_magnif(filt, _magnif);
	if (_window_size >= 0) 
		vl_sift_set_window_size(filt, _window_size);

	if (_verbose)
	{
		printf("vl_sift: filter settings:\n") ;
		printf("vl_sift:   octaves      (O)      = %d\n", vl_sift_get_noctaves(filt));
		printf("vl_sift:   levels       (S)      = %d\n", vl_sift_get_nlevels(filt));
		printf("vl_sift:   first octave (o_min)  = %d\n", vl_sift_get_octave_first(filt));
		printf("vl_sift:   edge thresh           = %g\n", vl_sift_get_edge_thresh(filt));
		printf("vl_sift:   peak thresh           = %g\n", vl_sift_get_peak_thresh(filt));
		printf("vl_sift:   norm thresh           = %g\n", vl_sift_get_norm_thresh(filt));
		printf("vl_sift:   window size           = %g\n", vl_sift_get_window_size(filt));
		printf("vl_sift:   float descriptor      = %d\n", floatDescriptors);
		printf("vl_sift: input keys provided? %s\n", _ikeys_provided ? "yes" : "no") ;
		printf((nikeys >= 0) ? "vl_sift: will source frames? yes (%d read)\n" : "vl_sift: will source frames? no\n", nikeys);
		printf("vl_sift: will force orientations? %s\n", force_orientations ? "yes" : "no");
	} 
	/* ...............................................................
	*                                             Process each octave
	* ............................................................ */

	i     = 0;
	first = 1;
	while (1) 
	{
		int                   err;
		VlSiftKeypoint const *keys  = 0;
		int                   nkeys = 0;

		if (_verbose)
		{
			printf ("vl_sift: processing octave %d\n", vl_sift_get_octave_index(filt));
		}

		/* Calculate the GSS for the next octave .................... */
		if (first) 
		{
			err   = vl_sift_process_first_octave(filt, data);
			first = 0;
		} 
		else 
		{
			err   = vl_sift_process_next_octave(filt);
		}

		if (err) 
			break;

		if (_verbose > 1)
		{
			printf("vl_sift: GSS octave %d computed\n", vl_sift_get_octave_index(filt));
		}

		/* Run detector ............................................. */
		if (nikeys < 0)
		{
			vl_sift_detect(filt);
			keys  = vl_sift_get_keypoints(filt);
			nkeys = vl_sift_get_nkeypoints(filt);
			i     = 0;

			if (_verbose > 1) 
			{
				printf ("vl_sift: detected %d (unoriented) keypoints\n", nkeys);
			}
		}
		else
		{
			nkeys = nikeys;
		}
		
		/* For each keypoint ........................................ */
		for (; i<nkeys; ++i)
		{
			double                angles[4];
			int                   nangles;
			VlSiftKeypoint        ik;
			VlSiftKeypoint const *k;

			/* Obtain keypoint orientations ........................... */
			if (nikeys >= 0)
			{	

				//if(i<5)
				//{	
				//	std::cout << "vlsift frame: " << i << std::endl;
				//	std::cout << "x: " << ikeys[4 * i + 0] << std::endl;
				//	std::cout << "y: " << ikeys[4 * i + 1] << std::endl;
				//	std::cout << "scale: " << ikeys[4 * i + 2] << std::endl;
				//	std::cout << "angle: " << ikeys[4 * i + 3] << std::endl;
				//}

				vl_sift_keypoint_init(filt, &ik, ikeys[4 * i + 1] - 1, ikeys[4 * i + 0] - 1, ikeys[4 * i + 2]);

				if (ik.o != vl_sift_get_octave_index(filt))
					break;

				k = &ik;

				/* optionally compute orientations too */
				if (force_orientations) 
				{
					nangles = vl_sift_calc_keypoint_orientations(filt, angles, k);
				} 
				else
				{
					angles[0] = VL_PI/2 - ikeys[4*i + 3];
					nangles   = 1;
				}
			} 
			else
			{
				k = keys + i;
				nangles = vl_sift_calc_keypoint_orientations(filt, angles, k);
			}

			/* For each orientation ................................... */
			for(q=0; q<nangles; ++q) 
			{
				vl_sift_pix  buf[128];
				vl_sift_pix rbuf[128];

				/* compute descriptor (if necessary) */
				vl_sift_calc_keypoint_descriptor(filt, buf, k, angles[q]);
				vlfeat_helperlib::transpose_descriptor(rbuf, buf);

				/* make enough room for all these keypoints and more */
				if (reserved < (*o_nframes) + 1)
				{
					reserved  += 2*nkeys;
					frames = (double*)realloc(frames, 4*sizeof(double)*reserved);
					if(!floatDescriptors)
					{
						descr  = (vl_uint8*)realloc(descr,  128*sizeof(vl_uint8)*reserved);
						std::cout << "Allocated memory for " << reserved << " vl_uint8 variables" << std::endl;
					}
					else
					{
						descr  = (float*)realloc(descr,  128*sizeof(float)*reserved);
						std::cout << "Allocated memory for 128*" << reserved << " float variables" << std::endl;
					}
				}

				/* Save back with MATLAB conventions. Notice that the input
				* image was the transpose of the actual image. */
				frames[4*(*o_nframes) + 0] = k->y + 1;
				frames[4*(*o_nframes) + 1] = k->x + 1;
				frames[4*(*o_nframes) + 2] = k->sigma;
				frames[4*(*o_nframes) + 3] = VL_PI/2 - angles[q];

				/*************************** DEBUG *******************/
				if(*o_nframes < 5)
				{	
					//debug(*o_nframes);
					//debug(frames[4*(*o_nframes) + 0]);
					//debug(frames[4*(*o_nframes) + 1]);
					//debug(frames[4*(*o_nframes) + 2]);
					//debug(frames[4*(*o_nframes) + 3]);

				}
				/*************************** DEBUG *******************/
				if(!floatDescriptors)
				{
					for(j=0; j<128; ++j)
					{
						float x = 512.0F * rbuf[j];
						x = (x < 255.0F) ? x : 255.0F;
						((vl_uint8*)descr)[128 * (*o_nframes) + j] = (vl_uint8)x;
					}
				}
				else
				{
					for (j=0; j<128; ++j)
					{
						//float x = rbuf[j];
						float x = 512.0F * rbuf[j];
						((float*)descr) [128 * (*o_nframes) + j] = x;
						/*if(*o_nframes<1)
						{
							std::cout << "j: " << j << "value: " << x << std::endl;
						}*/
					}
				}
				++(*o_nframes);
			} /* next orientation */
		} /* next keypoint */
	} /* next octave */
 


	if (_verbose)
		printf("vl_sift: found %d keypoints\n", (*o_nframes));

	// save variables:
	memcpy(o_DATAframes, frames, 4*(*o_nframes)*sizeof(double));
	if(!floatDescriptors)
		memcpy((vl_uint8*)o_DATAdescr, descr, 128*(*o_nframes)*sizeof(vl_uint8));
	else
		memcpy((float*)o_DATAdescr, descr, 128*(*o_nframes)*sizeof(float));
	/* cleanup */
	vl_sift_delete(filt);
	if(_ikeys_provided)
	{	
		free(ikeys);	//free's also ikeys_array
	//	free(ikeys_array);
	}
	/* end: do job */
	return;
}

/*********************************************************************************************************************/

void vlfeat_helperlib::transpose_descriptor(vl_sift_pix* dst, vl_sift_pix* src)
{
	int const BO = 8;  /* number of orientation bins */
	int const BP = 4;  /* number of spatial bins     */
	int i, j, t;

	for (j=0; j<BP; ++j)
	{
		int jp = BP-1-j;
		for (i=0; i<BP; ++i) 
		{
			int o  = BO*i + BP*BO*j;
			int op = BO*i + BP*BO*jp;
			dst[op] = src[o];
			for (t=1 ; t<BO; ++t)
			{
				dst[BO-t+op] = src[t+o];
			}
		}
	}
}

/*********************************************************************************************************************/

void vlfeat_helperlib::vlmatch(vl_uint8* L1_pt,vl_uint8* L2_pt, int K1, int K2, double thresh, int* nMatches, double* MATCHES )
{ 
    //Match descriptors!
    int ND = 128;
 
    Pair* pairs_begin = (Pair*) malloc(sizeof(Pair) * (K1+K2)) ;
    Pair* pairs_iterator = pairs_begin ; 
 
    int k1, k2;                                                       
 
    const int maxval = 0x7fffffff ;                        
    for(k1=0; k1<K1; ++k1, L1_pt += ND)
	{                    
        int best = maxval;                                    
        int second_best = maxval;                             
        int bestk = -1 ;                                                 
 
        /* For each point P2[k2] in the second image... */              
        for(k2=0; k2<K2; ++k2, L2_pt  += ND)
		{                      
            int bin;
            int acc = 0;
            for(bin=0; bin<ND; ++bin)
			{                              
                int delta = ((int) L1_pt[bin]) - ((int) L2_pt[bin]);                             
                acc  += delta*delta;                                         
            }
            
			/* Filter the best and second best matching point. */
            if(acc < best)
			{          
                second_best = best;                                         
                best = acc; 
                bestk = k2;
            }
			else if(acc < second_best)
			{                                 
                second_best = acc;
            }
        }
        L2_pt -= ND*K2;

		/* Lowe's method: accept the match only if unique. */
		if(thresh*(float)best < (float)second_best && bestk!=-1)
		{
			pairs_iterator->k1 = k1;
			pairs_iterator->k2 = bestk;
			pairs_iterator->score = best;
			pairs_iterator++;
			(*nMatches)++; //todo: ?rly
		}
	}
    Pair* pairs_end = pairs_iterator;
 
    //double* M_pt = (double*)calloc((pairs_end-pairs_begin)*2,sizeof(double));
    double* M_pt = (double*)calloc((*nMatches)*2,sizeof(double));
    //double* M_start = M_pt;
 
    for(pairs_iterator = pairs_begin; pairs_iterator<pairs_end; ++pairs_iterator) {
            *M_pt++ = pairs_iterator->k1 + 1;
            *M_pt++ = pairs_iterator->k2 + 1;
    }
 
    M_pt -= (*nMatches)*2;
    memcpy(MATCHES, M_pt, (*nMatches)*2*sizeof(double));
    free(pairs_begin);
    free(M_pt);
   
    return; 
}

/*********************************************************************************************************************/

static int vlfeat_helperlib::korder(void const* a, void const* b)
{
  double x = ((double*)a)[2] - ((double*)b)[2];
  if (x < 0)
	  return -1;
  if (x > 0) 
	  return +1;
  return 0;
}

/*********************************************************************************************************************/

vl_bool vlfeat_helperlib::check_sorted(double const * keys, vl_size nkeys)
{
  vl_uindex k;
  for (k=0; k + 1<nkeys; ++k)
  {
    if (vlfeat_helperlib::korder(keys, keys + 4) > 0)
	{
      return VL_FALSE ;
    }
    keys += 4;
  }
  return VL_TRUE;
}

/*********************************************************************************************************************/

// inline float vlfeat_helperlib::normalize_histogram(float* begin, float* end)
//{
//	float* iter;
//	float norm = 0.0f;
//
//	for (iter = begin; iter != end; ++iter)
//		norm += (*iter)*(*iter);
//
//	norm = vl_fast_sqrt_f(norm) + VL_EPSILON_F;
//	//norm = sqrt(norm) + VL_EPSILON_F;
//
//	for (iter = begin; iter != end; ++iter)
//		*iter /= norm;
//
//	return norm;
//}
//
///*********************************************************************************************************************/
//
//static inline float vlfeat_helperlib::vl_fast_resqrt_f(float x)
//{
//	/* 32-bit version */
//	union 
//	{
//		float x;
//		int  i;
//	}u;
//
//	float xhalf = (float)0.5*x;
//
//	/* convert floating point value in RAW integer */
//	u.x = x;
//
//	/* gives initial guess y0 */
//	u.i = 0x5f3759df - (u.i >> 1);
//
//	/* two Newton steps */
//	u.x = u.x * ( (float) 1.5  - xhalf*u.x*u.x);
//	u.x = u.x * ( (float) 1.5  - xhalf*u.x*u.x);
//	return u.x;
//}
//
///*********************************************************************************************************************/
//
//static inline float vlfeat_helperlib::vl_fast_sqrt_f(float x)
//{
//	return (x < 1e-8) ? 0 : x * vl_fast_resqrt_f(x);
//}