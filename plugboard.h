#pragma once
//Razoring
extern int razoring_margin1;
extern int razoring_margin2;
extern int razoring_depth ;

//LMR
// full depth moves counter
extern int full_depth_moves;
// depth limit to consider reduction
extern int lmr_depth ;
extern int lmr_fixed_reduction ;
extern int lmr_ratio ;

//Move ordering
extern int Bad_capture_score;

//Rfp
extern int rfp_depth ;
extern int rfp_score ;

//NMP
extern int nmp_depth ;
extern int nmp_fixed_reduction ;
extern int nmp_depth_ratio ;

//Movecount
extern int movecount_depth ;
// quiet_moves.count > (depth * movecount_multiplier)
extern int movecount_multiplier;

//Asp windows
extern int delta;
extern int Aspiration_Depth;
extern int Resize_limit;
extern int window_fixed_increment;
extern int window_resize_ratio;

//Evaluation pruning 
extern int ep_depth;
extern int ep_margin;