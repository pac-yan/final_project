/*========== my_main.c ==========
  This is the only file you need to modify in order
  to get a working mdl project (for now).
  my_main.c will serve as the interpreter for mdl.
  When an mdl script goes through a lexer and parser,
  the resulting operations will be in the array op[].
  Your job is to go through each entry in op and perform
  the required action from the list below:
  push: push a new origin matrix onto the origin stack
  pop: remove the top matrix on the origin stack
  move/scale/rotate: create a transformation matrix
                     based on the provided values, then
                     multiply the current top of the
                     origins stack by it.
  box/sphere/torus: create a solid object based on the
                    provided values. Store that in a
                    temporary matrix, multiply it by the
                    current top of the origins stack, then
                    call draw_polygons.
  line: create a line based on the provided values. Stores
        that in a temporary matrix, multiply it by the
        current top of the origins stack, then call draw_lines.
  save: call save_extension with the provided filename
  display: view the image live
  =========================*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "parser.h"
#include "symtab.h"
#include "y.tab.h"

#include "matrix.h"
#include "ml6.h"
#include "display.h"
#include "draw.h"
#include "stack.h"
#include "gmath.h"


/*======== void first_pass() ==========
  Inputs:
  Returns:
  Checks the op array for any animation commands
  (frames, basename, vary)
  Should set num_frames and basename if the frames
  or basename commands are present
  If vary is found, but frames is not, the entire
  program should exit.
  If frames is found, but basename is not, set name
  to some default value, and print out a message
  with the name being used.
  ====================*/
void first_pass() {
  //in order to use name and num_frames throughout
  //they must be extern variables
  extern int num_frames;
  extern char name[128];
  int has_f = 0, has_v = 0, has_n = 0;
  int x;
  num_frames = 1;
  for(x = 0; x < lastop; x++)
    {
      switch (op[x].opcode)
	{
	  case FRAMES :
	    if(has_f)
	      {
		printf("ERROR: Found multple FRAMES commands!\n");
		exit(0);
	      }
	    has_f = 1;
	    num_frames = op[x].op.frames.num_frames;
	    break;
	case BASENAME :
	  if(has_n)
	    {
	      printf("ERROR: Found multiple BASENAME commands!\n");
	      exit(0);
	    }
	  has_n = 1;
	  strcpy(name, op[x].op.basename.p->name);
	  break;
	case VARY :
	  has_v = 1;
	  break;
	}
    }
  if (has_v && !has_f) {
    printf("error: vary without frames\n");
    exit(0);
  }
  if (has_f && !has_n) {
    strcpy(name, "x");
    printf("frames without name: defaulting to \"x\"\n");
  }
}

/*======== struct vary_node ** second_pass() ==========
  Inputs:
  Returns: An array of vary_node linked lists
  In order to set the knobs for animation, we need to keep
  a seaprate value for each knob for each frame. We can do
  this by using an array of linked lists. Each array index
  will correspond to a frame (eg. knobs[0] would be the first
  frame, knobs[2] would be the 3rd frame and so on).
  Each index should contain a linked list of vary_nodes, each
  node contains a knob name, a value, and a pointer to the
  next node.
  Go through the opcode array, and when you find vary, go
  from knobs[0] to knobs[frames-1] and add (or modify) the
  vary_node corresponding to the given knob with the
  appropirate value.
  ====================*/
struct vary_node ** second_pass(){
  struct vary_node **knobs = calloc(num_frames, sizeof(struct vary_node *));
  int x, y; 
  for(x = 0; x < lastop; x++)
    {
      if (op[x].opcode == VARY) {
	double start_frame = op[x].op.vary.start_frame,
	  end_frame = op[x].op.vary.end_frame,
	  start_val = op[x].op.vary.start_val,
	  end_val = op[x].op.vary.end_val;
	char *name = op[x].op.vary.p->name;
	if (start_frame<0 || end_frame>=num_frames || start_frame>end_frame) {
	  printf("error: invalid frame range\n");
	  exit(0);
	}
	double delta_val = (end_val-start_val)/(end_frame-start_frame);
	for (y = start_frame; y <= end_frame; y++) {
	  struct vary_node *new = (struct vary_node *)malloc(sizeof(struct vary_node));
	  struct vary_node *old = knobs[y];
	  strcpy(new->name, name);
	  new->value = start_val+(y-start_frame)*delta_val;
	  new->next = old;
	  knobs[y] = new;
	}
      }
    }
  for (x=0; x< num_frames; x++) {
    struct vary_node *n = knobs[x];
    if (!knobs[x]) {
      printf("error: frames not covered by vary\n");
      exit(0);
    }
    while (n) {
      n = n->next;
    }
  }
  return knobs;
}

/*======== void print_knobs() ==========
Inputs:
Returns:
Goes through symtab and display all the knobs and their
currnt values
====================*/
void print_knobs() {
  int i;

  printf( "ID\tNAME\t\tTYPE\t\tVALUE\n" );
  for ( i=0; i < lastsym; i++ ) {

    if ( symtab[i].type == SYM_VALUE ) {
      printf( "%d\t%s\t\t", i, symtab[i].name );

      printf( "SYM_VALUE\t");
      printf( "%6.2f\n", symtab[i].s.value);
    }
  }
}

double get_val(struct vary_node *n, char *name) {
	while (n) {
		if (!strcmp(n->name, name)) return n->value;
		n = n->next;
	}
	printf("ERROR: Invalid knob name found!\n");
	exit(0);
}

/*======== void my_main() ==========
  Inputs:
  Returns:
  This is the main engine of the interpreter, it should
  handle most of the commadns in mdl.
  If frames is not present in the source (and therefore
  num_frames is 1, then process_knobs should be called.
  If frames is present, the enitre op array must be
  applied frames time. At the end of each frame iteration
  save the current screen to a file named the
  provided basename plus a numeric string such that the
  files will be listed in order, then clear the screen and
  reset any other data structures that need it.
  Important note: you cannot just name your files in
  regular sequence, like pic0, pic1, pic2, pic3... if that
  is done, then pic1, pic10, pic11... will come before pic2
  and so on. In order to keep things clear, add leading 0s
  to the numeric portion of the name. If you use sprintf,
  you can use "%0xd" for this purpose. It will add at most
  x 0s in front of a number, if needed, so if used correctly,
  and x = 4, you would get numbers like 0001, 0002, 0011,
  0487
  ====================*/
void my_main() {

  int x,y;
  struct vary_node **knobs;
  struct matrix *tmp;
  struct stack *systems;
  screen t;
  zbuffer zb;
  color g;
  g.red = 0;
  g.green = 0;
  g.blue = 0;
  double step_3d = 20;
  double theta;
  double knob_value, xval, yval, zval, val;

  SYMTAB *tab;

  //Lighting values here for easy access
  color ambient;
  double light[2][3];
  double view[3];
  double areflect[3];
  double dreflect[3];
  double sreflect[3];

  ambient.red = 50;
  ambient.green = 50;
  ambient.blue = 50;

  light[LOCATION][0] = 0.5;
  light[LOCATION][1] = 0.75;
  light[LOCATION][2] = 1;

  light[COLOR][RED] = 0;
  light[COLOR][GREEN] = 255;
  light[COLOR][BLUE] = 255;

  view[0] = 0;
  view[1] = 0;
  view[2] = 1;

  areflect[RED] = 0.1;
  areflect[GREEN] = 0.1;
  areflect[BLUE] = 0.1;

  dreflect[RED] = 0.5;
  dreflect[GREEN] = 0.5;
  dreflect[BLUE] = 0.5;

  sreflect[RED] = 0.5;
  sreflect[GREEN] = 0.5;
  sreflect[BLUE] = 0.5;

  systems = new_stack();
  tmp = new_matrix(4, 1000);
  clear_screen( t );
  clear_zbuffer(zb);
  
  first_pass();
  if (num_frames > 1) knobs = second_pass();
  for (y = 0; y < num_frames; y ++) {
    printf("Frame %d:03d/%03d:\n", y + 1, num_frames);
    if (num_frames > 1) {
      struct vary_node *current_node = knobs[y];
      while (current_node) {
	set_value(lookup_symbol(current_node->name), current_node->value);
	current_node = current_node->next;
      }
      //print_knobs();
    }
    for (x=0;x<lastop;x++) {
      //printf("%d: ",x);
      switch (op[x].opcode)
	{
	case LIGHT:
	  {
	    tab = op[x].op.light.p;
	    light[LOCATION][0] = tab->s.l->l[0];
	    light[LOCATION][1] = tab->s.l->l[1];
	    light[LOCATION][2] = tab->s.l->l[2];
	    light[COLOR][RED] =  tab->s.l->c[0];
	    light[COLOR][GREEN] =  tab->s.l->c[1];
	    light[COLOR][BLUE] =  tab->s.l->c[2];
	    break;
	  }
	case AMBIENT:
	  ambient.red = op[x].op.ambient.c[0];
	  ambient.green = op[x].op.ambient.c[1];
	  ambient.blue = op[x].op.ambient.c[2];
	  break;
	case SPHERE:
	  if (op[x].op.sphere.constants != NULL)
          {
	    tab = op[x].op.sphere.constants;
	    areflect[RED] = tab->s.c->r[0];
	    areflect[GREEN] = tab->s.c->g[0];
	    areflect[BLUE] = tab->s.c->b[0];
	    
	    dreflect[RED] = tab->s.c->r[1];
	    dreflect[GREEN] = tab->s.c->g[1];
	    dreflect[BLUE] = tab->s.c->b[1];
	    
	    sreflect[RED] = tab->s.c->r[2];
	    sreflect[GREEN] = tab->s.c->g[2];
	    sreflect[BLUE] = tab->s.c->b[2];
          }
	  if (op[x].op.sphere.cs != NULL)
	    {
	      //printf("\tcs: %s",op[x].op.sphere.cs->name);
	    }
	  add_sphere(tmp, op[x].op.sphere.d[0],
		     op[x].op.sphere.d[1],
		     op[x].op.sphere.d[2],
		     op[x].op.sphere.r, step_3d);
	  matrix_mult( peek(systems), tmp );
	  draw_polygons(tmp, t, zb, view, light, ambient,
			areflect, dreflect, sreflect);
	  tmp->lastcol = 0;
	  break;
	case TORUS:
	  if (op[x].op.torus.constants != NULL)
	    {
	      tab = op[x].op.torus.constants;
	      areflect[RED] = tab->s.c->r[0];
	      areflect[GREEN] = tab->s.c->g[0];
	      areflect[BLUE] = tab->s.c->b[0];
	      
	      dreflect[RED] = tab->s.c->r[1];
	      dreflect[GREEN] = tab->s.c->g[1];
	      dreflect[BLUE] = tab->s.c->b[1];
	      
	      sreflect[RED] = tab->s.c->r[2];
	      sreflect[GREEN] = tab->s.c->g[2];
	      sreflect[BLUE] = tab->s.c->b[2];
	    }
	  if (op[x].op.torus.cs != NULL)
	    {
	      //printf("\tcs: %s",op[x].op.torus.cs->name);
	    }
	  add_torus(tmp,
		    op[x].op.torus.d[0],
		    op[x].op.torus.d[1],
		    op[x].op.torus.d[2],
		    op[x].op.torus.r0,op[x].op.torus.r1, step_3d);
	  matrix_mult( peek(systems), tmp );
	  draw_polygons(tmp, t, zb, view, light, ambient,
			areflect, dreflect, sreflect);
	  tmp->lastcol = 0;
	  break;
	case CYLINDER:
	  if (op[x].op.cylinder.constants != NULL)
	    {
	      //printf("\tconstants: %s",op[x].op.cylinder.constants->name);
	    }
	  if (op[x].op.cylinder.cs != NULL)
	    {
	      //printf("\tcs: %s",op[x].op.cylinder.cs->name);
	    }
	  add_cylinder(tmp,
		       op[x].op.cylinder.d[0],
		       op[x].op.cylinder.d[1],
		       op[x].op.cylinder.d[2],
		       op[x].op.cylinder.r,op[x].op.cylinder.h, step_3d);
	  matrix_mult( peek(systems), tmp );
	  draw_polygons(tmp, t, zb, view, light, ambient, areflect, dreflect, sreflect);
	  tmp->lastcol = 0;
	  break;  
	case CONE:
	  if (op[x].op.cone.constants != NULL)
	    {
	      //printf("\tconstants: %s",op[x].op.cylinder.constants->name);
	    }
	  if (op[x].op.cone.cs != NULL)
	    {
	      //printf("\tcs: %s",op[x].op.cylinder.cs->name);
	    }
	  add_cone(tmp,
		   op[x].op.cone.d[0],
		   op[x].op.cone.d[1],
		   op[x].op.cone.d[2],
		   op[x].op.cone.r,op[x].op.cone.h, step_3d);
	  matrix_mult(peek(systems),tmp);
	  draw_polygons(tmp, t, zb, view, light, ambient, areflect, dreflect, sreflect);
	  tmp->lastcol = 0;
	  break;
	case BOX:
	  if (op[x].op.box.constants != NULL)
	    {
	      tab = op[x].op.box.constants;
	      areflect[RED] = tab->s.c->r[0];
	      areflect[GREEN] = tab->s.c->g[0];
	      areflect[BLUE] = tab->s.c->b[0];
	      
	      dreflect[RED] = tab->s.c->r[1];
	      dreflect[GREEN] = tab->s.c->g[1];
	      dreflect[BLUE] = tab->s.c->b[1];
	      
	      sreflect[RED] = tab->s.c->r[2];
	      sreflect[GREEN] = tab->s.c->g[2];
	      sreflect[BLUE] = tab->s.c->b[2];
	    }
	  if (op[x].op.box.cs != NULL)
	    {
	      //printf("\tcs: %s",op[x].op.box.cs->name);
	    }
	  add_box(tmp,
		  op[x].op.box.d0[0],op[x].op.box.d0[1],
		  op[x].op.box.d0[2],
		  op[x].op.box.d1[0],op[x].op.box.d1[1],
		  op[x].op.box.d1[2]);
	  matrix_mult( peek(systems), tmp );
	  draw_polygons(tmp, t, zb, view, light, ambient,
			areflect, dreflect, sreflect);
	  tmp->lastcol = 0;
	  break;
	case LINE:
	  if (op[x].op.line.constants != NULL)
	    {
	      //printf("\n\tConstants: %s",op[x].op.line.constants->name);
	    }
	  if (op[x].op.line.cs0 != NULL)
	    {
	      //printf("\n\tCS0: %s",op[x].op.line.cs0->name);
	    }
	  if (op[x].op.line.cs1 != NULL)
	    {
	      //printf("\n\tCS1: %s",op[x].op.line.cs1->name);
	    }
	  add_edge(tmp,
		   op[x].op.line.p0[0],op[x].op.line.p0[1],
		   op[x].op.line.p0[2],
		   op[x].op.line.p1[0],op[x].op.line.p1[1],
		   op[x].op.line.p1[2]);
	  matrix_mult( peek(systems), tmp );
	  draw_lines(tmp, t, zb, g);
	  tmp->lastcol = 0;
	  break;
	case MOVE:
	  xval = op[x].op.move.d[0];
	  yval = op[x].op.move.d[1];
	  zval = op[x].op.move.d[2];
	  printf("Move: %6.2f %6.2f %6.2f",
		 xval, yval, zval);
	  if (op[x].op.move.p != NULL)
	    {
	      tab = op[x].op.move.p;
	      val = tab->s.value;
	      xval *= val;
	      yval *= val;
	      zval *= val;
	    }
	  tmp = make_translate( xval, yval, zval );
	  matrix_mult(peek(systems), tmp);
	  copy_matrix(tmp, peek(systems));
	  tmp->lastcol = 0;
	  break;
	case SCALE:
	  xval = op[x].op.scale.d[0];
	  yval = op[x].op.scale.d[1];
	  zval = op[x].op.scale.d[2];
	  if (op[x].op.scale.p != NULL)
	    {
	      tab = op[x].op.scale.p;
	      val = tab->s.value;
	      xval *= val;
	      yval *= val;
	      zval *= val;
	    }
	  tmp = make_scale( xval, yval, zval );
	  matrix_mult(peek(systems), tmp);
	  copy_matrix(tmp, peek(systems));
	  tmp->lastcol = 0;
	  break;
	case ROTATE:
	  xval = op[x].op.rotate.axis;
	  theta = op[x].op.rotate.degrees;
	  if (op[x].op.rotate.p != NULL)
	    {
	      tab = op[x].op.rotate.p;
	      val = tab->s.value;
	      xval *= val;
	      theta *= val;
	    }
	  theta*= (M_PI / 180);
	  if (op[x].op.rotate.axis == 0 )
	    tmp = make_rotX( theta );
	  else if (op[x].op.rotate.axis == 1 )
	    tmp = make_rotY( theta );
	  else
	    tmp = make_rotZ( theta );
	  
	  matrix_mult(peek(systems), tmp);
	  copy_matrix(tmp, peek(systems));
	  tmp->lastcol = 0;
	  break;
	case PUSH:
	  //printf("Push");
	  push(systems);
	  break;
	case POP:
	  //printf("Pop");
	  pop(systems);
	  break;
	case SAVE:
	  //printf("Save: %s",op[x].op.save.p->name);
	  save_extension(t, op[x].op.save.p->name);
	  break;
	case DISPLAY:
	  //printf("Display");
	  display(t);
	  break;
	} //end opcode switch
      printf("\n");
    }//end operation loop
  }
}
