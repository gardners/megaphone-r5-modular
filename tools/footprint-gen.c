#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>

#define MAX_M 64  // Limit to 64-bit for unsigned long long

// Function to reverse bits in an unsigned long long
unsigned long long reverse_bits(unsigned long long num, int m) {
  unsigned long long rev = 0;
  for (int i = 0; i < m; i++) {
    if (num & (1ULL << i)) {
      rev |= (1ULL << (m - 1 - i));
    }
  }
  return rev;
}

// Function to compute flipped (mirrored) bit vector
unsigned long long flip_bits(unsigned long long num, int m) {
  unsigned long long flipped = 0;
  for (int i = 0; i < m / 2; i++) {
    int j = m/2 + i;
    bool left = num & (1ULL << i);
    bool right = num & (1ULL << j);
    if (left) flipped |= (1ULL << j);
    if (right) flipped |= (1ULL << i);
  }
  if (m % 2 == 1) { // Preserve middle bit if odd length
    flipped |= (num & (1ULL << (m / 2)));
  }
  return flipped;
}

// Function to check if a vector is unique under transformations
bool is_unique(unsigned long long value, unsigned long long *unique_set, int count, int m) {
  unsigned long long rotated = reverse_bits(value, m);
  unsigned long long flipped = flip_bits(value, m);
  unsigned long long flipped_rotated = reverse_bits(flipped, m);
    
  for (int i = 0; i < count; i++) {
    if (unique_set[i] == value || unique_set[i] == rotated || unique_set[i] == flipped || unique_set[i] == flipped_rotated) {
      return false;
    }
  }
  return true;
}

// Function to generate the n-th unique valid vector
unsigned long long find_nth_valid_vector(int m, int n, int target_index) {

  if (m > MAX_M) {
    fprintf(stderr,"ERROR: m exceeds maximum limit of %d\n", MAX_M);
    exit(-1);
  }
    
  int fixed_positions[] = {0, m - 1, m / 2 - 1, m / 2};
  int num_fixed = 4;
  int num_free_positions = m - num_fixed;
  int num_select = n - num_fixed;
    
  if (num_select > num_free_positions) {
    fprintf(stderr,"ERROR: Not enough free positions to select %d additional pins.\n", num_select);
    exit(-1);
  }
    
  unsigned long long unique_vectors[10000]; // Store unique solutions
  int unique_count = 0;
    
  // Iterate through all bit combinations for the free positions
  for (unsigned long long mask = 0; mask < (1ULL << num_free_positions); mask++) {
    if (__builtin_popcountll(mask) == num_select) {
      unsigned long long vector = 0;
            
      // Place the fixed positions
      for (int i = 0; i < num_fixed; i++) {
	vector |= (1ULL << fixed_positions[i]);
      }
            
      // Map selected positions to the bitmask
      int free_idx = 0;
      for (int i = 0; i < m; i++) {
	bool is_fixed = false;
	for (int j = 0; j < num_fixed; j++) {
	  if (i == fixed_positions[j]) {
	    is_fixed = true;
	    break;
	  }
	}
	if (!is_fixed && (mask & (1ULL << free_idx))) {
	  vector |= (1ULL << i);
	}
	if (!is_fixed) {
	  free_idx++;
	}
      }
            
      // Check if it's a unique configuration
      if (is_unique(vector, unique_vectors, unique_count, m)) {
	unique_vectors[unique_count++] = vector;
	if (unique_count == target_index) {
	  return vector;
	}
      }
    }
  }
    
  fprintf(stderr,"FATAL: Could not find the %d-th unique vector.\n", target_index);
  exit(-1);
}





#define MAX_PADS 64
int pad_present[MAX_PADS];

#define MAX_VARIANTS 1024
unsigned long long variant_pads[MAX_VARIANTS];

char suffix[128]="";
char footprint_name[1024];
char bay_footprint_name[1024];

unsigned long long find_variant(int half_pin_count,int pins_used,char *variant_str,
				float co_width,float co_height)
{

  unsigned int variant = atoi(variant_str);

  unsigned long long v=0;
  
  if (variant_str[0]=='m'||variant_str[0]=='M') {
    // Okay, at this point we know how many pins, and the user has requested a
    // specific pin pattern. So parse out the pin mask directly.
    // We should support both hex (since its how the files get named) and binary forms.
    // But for now, we'll just take hex.
    v = strtoll(&variant_str[1],NULL,16);
    fprintf(stderr,"INFO: Explicitly selected pin_mask = 0x%llx. No sanity checks made on this.\n",
	    v);
    if (half_pin_count&1) v=v>>2;
    
    // XXX - We could check for the corner pins being set, and the correct total number of bits being set
    
  } else {   
    if (variant<1) {
      fprintf(stderr,"ERROR: Variant number must be >=1\n");
      exit(-1);
    }
    v = find_nth_valid_vector(half_pin_count*2, pins_used, variant);
  }
  if (half_pin_count&1) {
    // pin count won't be a whole nybl, so shift left 2 bits first.
    sprintf(suffix,"M%llX",v<<2);
  } else 
    sprintf(suffix,"M%llX",v);
  sprintf(footprint_name,"MegaCastle2x%d-Module-I%.1fx%.1f-%s",
	  half_pin_count,co_width,co_height,
	  suffix);
  sprintf(bay_footprint_name,"MegaCastle2x%d-I%.1fx%.1f-%s",
	  half_pin_count,co_width,co_height,
	  suffix);
  return reverse_bits(v,half_pin_count*2);
}

int pin_present(int pin_num,unsigned long long vector)
{
  int shift = pin_num - 1;
  unsigned long long shifted = (vector >> shift);
  return shifted & 1;
}




void usage(void)
{
  fprintf(stderr,"footprint-gen <output path> <cut-out width> <cut-out height> <number of pins> <variant>\n");
  exit(-1);
}

void symbol_write(FILE *out,char *symbol,char *footprint,int bay, int half_pin_count, unsigned long long pin_mask, int edge_type, int with_cutout, char **argv)
{
  
  float symbol_height = (half_pin_count + 1) * 2.54;
  float symbol_top = (symbol_height / 2);
  float symbol_bottom = - ( symbol_height / 2 ); 

  fprintf(out,
	  "\t(symbol \"%s\"\n"
	  "                (exclude_from_sim no)\n"
	  "                (in_bom yes)\n"
	  "                (on_board yes)\n"
	  "                (property \"Reference\" \"M\"\n"
	  "                        (at 0 %f 0)\n"
	  "                        (effects\n"
	  "                                (font\n"
	  "                                        (size 1.27 1.27)\n"
	  "                                )\n"
	  "                        )\n"
	  "                )\n"
	  "                (property \"Value\" \"\"\n"
	  "                        (at 0 5.08 0)\n"
	  "                        (effects\n"
	  "                                (font\n"
	  "                                        (size 1.27 1.27)\n"
	  "                                )\n"
	  "                        )\n"
	  "                )\n"
	  "                (property \"Footprint\" \"MegaCastle:%s\"\n"
	  "                        (at 0 %f 0)\n"
	  "                        (effects\n"
	  "                                (font\n"
	  "                                        (size 1.27 1.27)\n"
	  "                                )\n"
	  "                                (hide yes)\n"
	  "                        )\n"
	  "                )\n"
	  "                (property \"Datasheet\" \"\"\n"
	  "                        (at 0 5.08 0)\n"
	  "                        (effects\n"
	  "                                (font\n"
	  "                                        (size 1.27 1.27)\n"
	  "                                )\n"
	  "                                (hide yes)\n"
	  "                        )\n"
	  "                )\n"
	  "                (property \"Description\" \"Generated using footprint-gen %s %s %s %s %s\"\n"
	  "                        (at 0 0 0)\n"
	  "                        (effects\n"
	  "                                (font\n"
	  "                                        (size 1.27 1.27)\n"
	  "                                )\n"
	  "                                (hide yes)\n"
	  "                        )\n"
	  "                )\n"
	  ,
	  symbol,
	  symbol_top - 2.54 * (half_pin_count + 2),
	  footprint,
	  symbol_top - 2.54 * (half_pin_count + (3 + bay)),
	  argv[1],argv[2],argv[3],argv[4],suffix
	  );

  // Output rectangles for the symbol and the GND and (if not edge version) VCC straps.
  
  fprintf(out,
	  "                (symbol \"%s_0_1\"\n",
	  symbol);

  if (bay) {
  
    if (!edge_type)
      fprintf(out,
	      "                        (rectangle\n"
	      "                                (start -27.94 16.51)\n"
	      "                                (end -12.7 10.16)\n"
	      "                                (stroke\n"
	      "                                        (width 0)\n"
	      "                                        (type default)\n"
	      "                                )\n"
	      "                                (fill\n"
	      "                                        (type none)\n"
	      "                                )\n"
	      "                        )\n"
	      );
    fprintf(out,
	    "                        (rectangle\n"
	    "                                (start -27.94 -8.89)\n"
	    "                                (end -12.7 -15.24)\n"
	    "                                (stroke\n"
	    "                                        (width 0)\n"
	    "                                        (type default)\n"
	    "                                )\n"
	    "                                (fill\n"
	    "                                        (type none)\n"
	    "                                )\n"
	    "                        )\n"
	    );
  }
  
  // Main rectangle for the pins
  fprintf(out,
	  "                        (rectangle\n"
	  "                                (start -6.35 %f)\n"
	  "                                (end 7.62 %f)\n"
	  "                                (stroke\n"
	  "                                        (width 0)\n"
	  "                                        (type default)\n"
	  "                                )\n"
	  "                                (fill\n"
	  "                                        (type none)\n"
	  "                                )\n"
	  "                        )\n"
	  ,
	  symbol_top,
	  symbol_bottom
	  );

  if (with_cutout) {
    // Main rectangle for the pins
    fprintf(out,
	    "                        (rectangle\n"
	    "                                (start -3.35 %f)\n"
	    "                                (end 4.62 %f)\n"
	    "                                (stroke\n"
	    "                                        (width 0)\n"
	    "                                        (type default)\n"
	    "                                )\n"
	    "                                (fill\n"
	    "                                        (type none)\n"
	    "                                )\n"
	    "                        )\n"
	    ,
	    symbol_top - 2.54*2,
	    symbol_bottom + 2.54*2
	    );
    
    
  }
  
  fprintf(out,
	  "                )\n"
	  );
  
  
  // Add descriptions for GND and VCC bridge blocks
  fprintf(out,
	  "                (symbol \"%s_1_1\"\n",
	  symbol
	  );
  
  if (bay) {
    fprintf(out,
  	    "                        (text \"GND Bridge\\n(Connect B1 to %d,\\nB2 to GND)\"\n"
	    "                                (at -17.78 -5.08 0)\n"
	    "                                (effects\n"
	    "                                        (font\n"
	    "                                                (size 1.27 1.27)\n"
	    "                                        )\n"
	    "                                )\n"
	    "                        )\n"
	    ,
	    half_pin_count
	    );
    if (!edge_type)
      fprintf(out,
	      "                        (text \"VCC Bridge\\n(Connect A1 to 1,\\nA2 to VCC)\"\n"
	      "                                (at -17.78 6.35 0)\n"
	      "                                (effects\n"
	      "                                        (font\n"
	      "                                                (size 1.27 1.27)\n"
	      "                                        )\n"
	      "                                )\n"
	      "                        )\n"
	      );
  }

  if (edge_type) {
    fprintf(out,
	    "                        (text \" BOARD EDGE\"\n"
	    "                                (at 0 %f 0)\n"
	    "                                (effects\n"
	    "                                        (font\n"
	    "                                                (size 1.27 1.27)\n"
	    "                                        )\n"
	    "                                )\n"
	    "                        )\n"
	    ,
	    symbol_top + 2.54 * 0.5
	    );

  }

  if (with_cutout) {
    fprintf(out,
	    "                        (text \"%s\"\n"
	    "                                (at 0.5 0 900)\n"
	    "                                (effects\n"
	    "                                        (font\n"
	    "                                                (size 1 1)\n"
	    "                                        )\n"
	    "                                )\n"
	    "                        )\n"
	    ,
	    bay ? "CUTOUT" : "REAR ZONE"
	    );

  }

  
  // Add pins down left hand side      
  for(int i=1;i<=half_pin_count;i++) {
    
    if (pin_present(i,pin_mask))	
      fprintf(out,
	      "                       (pin bidirectional line\n"
	      "                                (at -6.35 %f 0)\n"
	      "                                (length 2.54)\n"
	      "                                (name \"%s\"\n"
	      "                                        (effects\n"
	      "                                                (font\n"
	      "                                                        (size 1.27 1.27)\n"
	      "                                                )\n"
	      "                                        )\n"
	      "                                )\n"
	      "                                (number \"%d\"\n"
	      "                                        (effects\n"
	      "                                                (font\n"
	      "                                                        (size 1.27 1.27)\n"
	      "                                                )\n"
	      "                                        )\n"
	      "                                )\n"
	      "                        )\n"
	      ,
	      symbol_top - i * 2.54,
	      ((!edge_type) && (i==1)) ? "VCC" : ( ( i==half_pin_count ) ? "GND" : ""),
	      i
	      );	       	
  }
  
  for(int i=1;i<=half_pin_count;i++) {
    
    if (pin_present(i+half_pin_count,pin_mask))
      fprintf(out,
	      "                       (pin bidirectional line\n"
	      "                                (at 7.62 %f 180)\n"
	      "                                (length 2.54)\n"
	      "                                (name \"%s\"\n"
	      "                                        (effects\n"
	      "                                                (font\n"
	      "                                                        (size 1.27 1.27)\n"
	      "                                                )\n"
	      "                                        )\n"
	      "                                )\n"
	      "                                (number \"%d\"\n"
	      "                                        (effects\n"
	      "                                                (font\n"
	      "                                                        (size 1.27 1.27)\n"
	      "                                                )\n"
	      "                                        )\n"
	      "                                )\n"
	      "                        )\n"
	      ,
	      symbol_top - i * 2.54,
	      "",
	      i+half_pin_count
	      );	       	
  }

  if (bay) {
    if (!edge_type)
      fprintf(out,
	      "                       (pin bidirectional line\n"
	      "                                (at -12.7 11.43 180)\n"
	      "                                (length 2.54)\n"
	      "                                (name \"Module_VCC\"\n"
	      "                                        (effects\n"
	      "                                                (font\n"
	      "                                                        (size 1.27 1.27)\n"
	      "                                                )\n"
	      "                                        )\n"
	      "                                )\n"
	      "                                (number \"A1\"\n"
	      "                                        (effects\n"
	      "                                                (font\n"
	      "                                                        (size 1.27 1.27)\n"
	      "                                                )\n"
	      "                                        )\n"
	      "                                )\n"
	      "                        )\n"
	      "                        (pin bidirectional line\n"
	      "                                (at -12.7 13.97 180)\n"
	      "                                (length 2.54)\n"
	      "                                (name \"Board_VCC\"\n"
	      "                                        (effects\n"
	      "                                                (font\n"
	      "                                                        (size 1.27 1.27)\n"
	      "                                                )\n"
	      "                                        )\n"
	      "                                )\n"
	      "                                (number \"A2\"\n"
	      "                                        (effects\n"
	      "                                                (font\n"
	      "                                                        (size 1.27 1.27)\n"
	      "                                                )\n"
	      "                                        )\n"
	      "                                )\n"
	      "                        )\n"
	      );
    
    fprintf(out,
	    "                       (pin bidirectional line\n"
	    "                                (at -12.7 -11.43 180)\n"
	    "                                (length 2.54)\n"
	    "                                (name \"Module_GND\"\n"
	    "                                        (effects\n"
	    "                                                (font\n"
	    "                                                        (size 1.27 1.27)\n"
	    "                                                )\n"
	    "                                        )\n"
	    "                                )\n"
	    "                                (number \"B1\"\n"
	    "                                        (effects\n"
	    "                                                (font\n"
	    "                                                        (size 1.27 1.27)\n"
	    "                                                )\n"
	    "                                        )\n"
	    "                                )\n"
	    "                        )\n"
	    "                        (pin bidirectional line\n"
	    "                                (at -12.7 -13.97 180)\n"
	    "                                (length 2.54)\n"
	    "                                (name \"Board_GND\"\n"
	    "                                        (effects\n"
	    "                                                (font\n"
	    "                                                        (size 1.27 1.27)\n"
	    "                                                )\n"
	    "                                        )\n"
	    "                                )\n"
	    "                                (number \"B2\"\n"
	    "                                        (effects\n"
	    "                                                (font\n"
	    "                                                        (size 1.27 1.27)\n"
	    "                                                )\n"
	    "                                        )\n"
	    "                                )\n"
	    "                        )\n"
	    );
  }
  
  fprintf(out,
	  "\t\t)\n"
	  );
  
  
  fprintf(out,
	  "\t)\n"
	  );
  
}


void draw_qr_corner(FILE *out,float x, float y, int draw_outer)
{
  fprintf(out,
	  "  (fp_rect (start %f %f) (end %f %f)\n"
	  "    (stroke (width 0) (type solid)) (fill solid) (layer \"F.SilkS\") (tstamp cf9cd8c1-cf12-4383-8e04-88ab007df94c))\n",
	  x-1.27,y-1.27,
	  x-2.54,y-2.54
	  );

  if (draw_outer) {
    fprintf(out,
	    "  (fp_rect (start %f %f) (end %f %f)\n"
	    "    (stroke (width 0.5) (type default)) (fill none) (layer \"F.SilkS\") (tstamp cf9cd8c1-cf12-4383-8e04-88ab007df94c))\n",
	    x,y,
	    x-3.81,y-3.81
	    );
  }
}
	  

void draw_line(FILE *out,char *layer,float x1, float y1, float x2, float y2, float width)
{
  fprintf(out,
	  "  (fp_line (start %f %f) (end %f %f)\n"
	  "    (stroke (width %f) (type default)) (layer \"%s\") (tstamp 2c385f8a-f9bf-4768-8345-7af626d58ca0))\n",
	  x1,y1,x2,y2,width,layer);
}

void draw_circle(FILE *out, char *layer, float x, float y, float diameter, float width)
{
  fprintf(out,
	  "         (fp_circle\n"
	  "                (center %f %f)\n"
	  "                (end %f %f)\n"
	  "                (stroke\n"
	  "                        (width %f)\n"
	  "                        (type default)\n"
	  "                )\n"
	  "                (fill none)\n"
	  "                (layer \"%s\")\n"
	  "                (uuid \"8b9af24b-14b8-4655-9676-2e957c2621b0\")\n"
	  "        )\n"
	  ,
	  x,y,x,y-diameter/2,width,layer);
}

void draw_drill_hole(FILE *out, float x, float y, float diameter)
{
  fprintf(out,
	  "       (pad \"\" np_thru_hole circle\n"
	  "               (at %f %f)\n"
	  "               (size %f %f)\n"
	  "               (drill %f)\n"
	  "               (layers \"F&B.Cu\" \"Edge.Cuts\")\n"
	  "               (uuid \"39334884-b6dc-45d8-b64f-0368d5b22370\")\n"
	  "       )\n"
	  ,
	  x,y,
	  diameter,
	  diameter,
	  diameter
	  );
}


void draw_arc(FILE *out, char *layer, float x1, float y1, float xm, float ym, float x2, float y2, float width)
{
  fprintf(out,
	  "       (fp_arc\n"
	  "                (start %f %f)\n"
	  "                (mid %f %f)\n"
	  "                (end %f %f)\n"
	  "                (stroke\n"
	  "                        (width 0.05)\n"
	  "                        (type default)\n"
	  "                )\n"
	  "                (layer \"%s\")\n"
	  "                (uuid \"2cfe695a-89f3-455b-b40e-f067e5aa3651\")\n"
	  "        )\n"
	  ,
	  x1,y1,xm,ym,x2,y2,
	  layer
	  );
  
}

void footprint_exclusion_zone(FILE *out, float x1, float y1, float x2, float y2)
{
  fprintf(out,
	  "       (zone\n"
	  "                (net 0)\n"
	  "                (net_name \"\")\n"
	  "                (layer \"B.Cu\")\n"
	  "                (uuid \"ae5d0a55-9783-4f23-9325-7c00d54e0c32\")\n"
	  "                (hatch edge 0.5)\n"
	  "                (connect_pads\n"
	  "                        (clearance 0)\n"
	  "                )\n"
	  "                (min_thickness 0.25)\n"
	  "                (filled_areas_thickness no)\n"
	  "                (keepout\n"
	  "                        (tracks allowed)\n"
	  "                        (vias allowed)\n"
	  "                        (pads allowed)\n"
	  "                        (copperpour allowed)\n"
	  "                        (footprints not_allowed)\n"
	  "                )\n"
	  "                (fill\n"
	  "                        (thermal_gap 0.5)\n"
	  "                        (thermal_bridge_width 0.5)\n"
	  "                )\n"
	  "                (polygon\n"
	  "                        (pts\n"
	  "                                (xy %f %f) (xy %f %f) (xy %f %f) (xy %f %f)\n"
	  "                        )\n"
	  "                )\n"
	  "        )\n",
	  x1,y1,
	  x1,y2,
	  x2,y2,
	  x2,y1
	  );
}

int main(int argc, char **argv)
{
  if (argc!=6) {
    usage();
  }

  char *path = argv[1];
  float co_width=atof(argv[2]);
  float co_height=atof(argv[3]);
  float pins_used=atof(argv[4]);
  // variant now gets parsed as a string
  // int variant=atoi(argv[5]);

  double module_height = co_height + 2.4 + 2.4;
  double module_width = co_width + 1.8 + 1.8;

  // Make module size integer 10ths of an inch
  double frac =  module_height - ((int)(module_height / 2.54))*2.54;
  if (frac > 0.001) module_height = module_height + (2.54 - frac);

  // Does it really make sense to make the width align to .1" grid?
  //  frac =  module_width - ((int)(module_width / 2.54))*2.54;
  //  if (frac > 0.001) module_width = module_width + (2.54 - frac);

  int half_pin_count = module_height / 2.54;

  while (pins_used >= (half_pin_count*2) ) {
    half_pin_count++;
  }
  
  while ( (half_pin_count*2-1) < pins_used ) half_pin_count++;

  module_height = half_pin_count * 2.54;

  // Increase cut-out height if we added more pins than the original size
  // required.
  co_height = module_height - (2.4 + 2.4);
  
  fprintf(stderr,"INFO: Module size = %fx%f mm, %d pins each side.\n",
	  module_width, module_height, half_pin_count);

  unsigned long long pin_mask = 0;

  // Assume variant field is just an ordinal number
  pin_mask = find_variant(half_pin_count,pins_used,argv[5],
			  co_width,co_height);  

  /* ------------------------------------------------------------------------------

     Module footprint

     ------------------------------------------------------------------------------ */

  FILE *out=NULL;
  char filename[8192];

  const float SPARE_HOLE_DISPLACEMENT = 1.7;

  for(int sparepad=0;sparepad<2;sparepad++) {
    for(int castellated=0;castellated<2;castellated++) {
      for(int panelised=0;panelised<2;panelised++) {
	
	snprintf(filename,8192,"%s/MegaCastle.pretty/%s%s%s%s.kicad_mod",
		 path,footprint_name,
		 panelised?"-PANEL":"",
		 castellated?"":"-NIBBLE",
		 sparepad?"-SPAREPAD":"");
	out = fopen(filename,"w");
	if (!out) {
	  fprintf(stderr,"ERROR: Failed to create file '%s'\n",filename);
	  perror("fopen");
	  exit(-1);
	}
	
	fprintf(stderr,"INFO: Creating %s\n",filename);
	
	fprintf(out,
		"(footprint \"%s%s\"\n"
		"        (version 20240108)\n"
		"	    (generator \"pcbnew\")\n"
		"        (generator_version \"8.0\")\n"
		"        (layer \"F.Cu\")\n"
		"        (property \"Reference\" \"REF**\"\n"
		"                (at 0 %f 0)\n"   // XXX - Set position of REF**
		"                (unlocked yes)\n"
		"	 (layer \"F.SilkS\")\n"
		"                (uuid \"08d1cbd6-6dbd-4696-9477-43e1380128be\")\n"
		"                (effects\n"
		"                        (font\n"
		"                                (size 1 1)\n"
		"                                (thickness 0.1)\n"
		"                        )\n"
		"                )\n"
		"        )\n",
		footprint_name,panelised?"-PANEL":"",
		module_height/2 + 2 + panelised * 4
		);
	
	fprintf(out,
		"        (property \"Value\" \"%s%s\"\n"
		"                (at 0 %f 0)\n"
		"                (unlocked yes)\n"
		"                (layer \"F.Fab\")\n"
		"                (uuid \"7848b12f-b698-40ff-9b18-5d3dc1ad63e0\")\n"
		"                (effects\n"
		"                        (font\n"
		"                                (size 1 1)\n"
		"                                (thickness 0.15)\n"
		"                        )\n"
		"                )\n"
		"        )\n",
		footprint_name,panelised?"-PANEL":"",
		module_height/2 + 4 + panelised * 4
		);
    
	fprintf(out,"        (property \"Footprint\" \"\"\n"
		"                (at 0 0 0)\n"
		"                (layer \"F.Fab\")\n"
		"                (hide yes)\n"
		"                (uuid \"24d79921-7c8a-4bf5-b40a-0bc77c603d9c\")\n"
		"                (effects\n"
		"                        (font\n"
		"                                (size 1.27 1.27)\n"
		"                                (thickness 0.15)\n"
		"                        )\n"
		"                )\n"
		"        )\n"
		);
    
	fprintf(out,
		"        (property \"Datasheet\" \"\"\n"
		"                (at 0 0 0)\n"
		"                (layer \"F.Fab\")\n"
		"                (hide yes)\n"
		"                (uuid \"1a070192-ad50-44fd-8acb-aa45a22d9533\")\n"
		"                (effects\n"
		"                        (font\n"
		"                                (size 1.27 1.27)\n"
		"                                (thickness 0.15)\n"
		"                        )\n"
		"                )\n"
		"       )\n"
		);
    
	fprintf(out,
		"                (property \"Description\" \"Generated using footprint-gen %s %s %s %s %s\"\n"
		"                (at 0 0 0)\n"
		"                (layer \"F.Fab\")\n"
		"                (hide yes)\n"
		"                (uuid \"d6dd6c62-1d04-4d09-a07a-575c94e93110\")\n"
		"                (effects\n"
		"                        (font\n"
		"                                (size 1.27 1.27)\n"
		"                                (thickness 0.15)\n"
		"                        )\n"
		"                )\n"
		"        )\n"
		"        (attr smd)\n",
		argv[1],argv[2],argv[3],argv[4],suffix
		);

	// Add 1/16th of an inch to castellated pads if fabricating for nibbling the edges to make the castellations.
	float cut_width = module_width/2;
	if (!castellated) cut_width += 25.4/16.0;
    
	if (panelised) {
	  float tab_width =  ( .1 + .6 + .3975 + .45 + .35 + .45 + .35 + .45 + .35 + .45 + .3975 + .6 + .1 );
	  int num_tabs=2;
	  // If not room for two tabs, have just one
	  if (module_width < ( 2+ tab_width + 2 + tab_width + 2 ) ) num_tabs=1;
	  float first_tab_offset= 2;
	  float second_tab_offset = module_width - 2 - tab_width;
	  if (num_tabs == 1 ) {
	    first_tab_offset = module_width/2 - tab_width/2;
	    second_tab_offset = first_tab_offset;
	  }

	  // Draw lines either side of the tab(s)
	  draw_line(out,"Edge.Cuts",-cut_width,module_height/2,-module_width/2 + first_tab_offset,module_height/2,0.05);
	  if (num_tabs == 2) {
	    draw_line(out,"Edge.Cuts",
		      -module_width/2 + first_tab_offset + tab_width,module_height/2,
		      -module_width/2 + second_tab_offset,module_height/2, 0.05);
	    draw_line(out,"Edge.Cuts",
		      -module_width/2 + second_tab_offset + tab_width,module_height/2,
		      cut_width,module_height/2, 0.05);
	  } else {
	    draw_line(out,"Edge.Cuts",
		      -module_width/2 + first_tab_offset + tab_width,module_height/2,
		      cut_width,module_height/2,0.05);		
	  }

	  for(int tab = 0;tab < num_tabs ; tab++ ) {
	    float tab_offset = first_tab_offset;
	    if (tab) tab_offset = second_tab_offset;

	    // Draw parallel lines around groove
	    draw_line(out,"Edge.Cuts",
		      -module_width/2 + tab_offset, module_height/2,
		      -module_width/2 + tab_offset + (.1 + 0.6/2), module_height/2,0.05);
	    draw_line(out,"Edge.Cuts",
		      -module_width/2 + tab_offset, module_height/2 + 1.6,
		      -module_width/2 + tab_offset + (.1 + 0.6/2), module_height/2 + 1.6,
		      0.05
		      );
	    draw_line(out,"Edge.Cuts",
		      -module_width/2 + tab_offset + tab_width, module_height/2,
		      -module_width/2 + tab_offset + tab_width - (.1 + 0.6/2), module_height/2, 0.05);
	    draw_line(out,"Edge.Cuts",
		      -module_width/2 + tab_offset + tab_width, module_height/2 + 1.6,
		      -module_width/2 + tab_offset + tab_width - (.1 + 0.6/2), module_height/2 + 1.6,
		      0.05
		      );

	    // Draw arcs connecting the lower and upper sides of the groove
	    draw_arc(out,"Edge.Cuts",
		     -module_width/2 + tab_offset + .4, module_height/2,
		     -module_width/2 + tab_offset + .4 + 0.8, module_height/2 + 0.8,		 
		     -module_width/2 + tab_offset + .4, module_height/2 + 1.6,
		     0.05);
	    draw_arc(out,"Edge.Cuts",
		     -module_width/2 + tab_offset + tab_width - 0.4, module_height/2,
		     -module_width/2 + tab_offset + tab_width - 0.4 - 0.8, module_height/2 + 0.8,		 
		     -module_width/2 + tab_offset + tab_width - 0.4, module_height/2 + 1.6,
		     0.05);

	    float circle_offsets[6]={.9225,.8,.8,.8,.9225,0};
	    float diameters[6]={.6,.45,.45,.45,.45,.6};
	    float offset=0.1 + diameters[0]/2;
	    for(int i=0;i<6;i++) {
	      draw_drill_hole(out,
			      -module_width/2 + tab_offset + offset,
			      module_height/2,
			      diameters[i]);
	      offset += circle_offsets[i];
	    }		 
	  }
      
	} else {
	  // Non-panelised, so just draw the line at the bottom
	  draw_line(out,"Edge.Cuts",-module_width/2,module_height/2,module_width/2,module_height/2,0.05);
	}


	// Top Edge cut
	draw_line(out,"Edge.Cuts",-cut_width,-module_height/2,cut_width,-module_height/2,0.05);

	// Sides
	draw_line(out,"Edge.Cuts",cut_width,-module_height/2,cut_width,module_height/2,0.05);
	draw_line(out,"Edge.Cuts",-cut_width,-module_height/2,-cut_width,module_height/2,0.05);
    
	// Labels for PRY zones
	fprintf(out,
		"        (fp_text user \"PRY\"\n"
		"                (at 0 %.2f 0)\n"
		"                (unlocked yes)\n"
		"                (layer \"B.SilkS\")\n"
		"                (uuid \"1c67120b-cd38-4dc1-b421-22e46cf03846\")\n"
		"                (effects\n"
		"                        (font\n"
		"                                (size 0.8 0.8)\n"
		"                                (thickness 0.1)\n"
		"                        )\n"
		"                        (justify bottom mirror)\n"
		"                )\n"
		"        )\n",
		-module_height/2.0 + 0.8 + 0.5
		);
	fprintf(out,
		"        (fp_text user \"PRY\"\n"
		"                (at 0 %.2f 0)\n"
		"                (unlocked yes)\n"
		"                (layer \"B.SilkS\")\n"
		"                (uuid \"1c67120b-cd38-4dc1-b421-22e46cf03846\")\n"
		"                (effects\n"
		"                        (font\n"
		"                                (size 0.8 0.8)\n"
		"                                (thickness 0.1)\n"
		"                        )\n"
		"                        (justify bottom mirror)\n"
		"                )\n"
		"        )\n",
		module_height/2.0 - 0.3
		);
    
	// Rectangles for PRY zones
	float pry_height = 6.268592 - 4.8;
	fprintf(out,
		"	 (fp_rect\n"
		"                (start -1.984241 %.3f)\n"
		"                (end 2.019516 %.3f)\n"
		"                (stroke\n"
		"                        (width 0.1)\n"
		"                        (type default)\n"
		"                )\n"
		"                (fill none)\n"
		"                (layer \"B.SilkS\")\n"
		"                (uuid \"966a509b-00f8-4763-8fcb-dbbd2ac08dcd\")\n"
		"        )\n",
		module_height/2 - 0.08 - pry_height,
		module_height/2 - 0.08
		);
	fprintf(out,
		"	 (fp_rect\n"
		"                (start -1.984241 %.3f)\n"
		"                (end 2.019516 %.3f)\n"
		"                (stroke\n"
		"                        (width 0.1)\n"
		"                        (type default)\n"
		"                )\n"
		"                (fill none)\n"
		"                (layer \"B.SilkS\")\n"
		"                (uuid \"966a509b-00f8-4763-8fcb-dbbd2ac08dcd\")\n"
		"        )\n",
		-module_height/2 + 0.08 + pry_height,
		-module_height/2 + 0.08
		);
    
	// Component and track exclusion zones for pry zones
	fprintf(out,
		"	 (zone\n"
		"                (net 0)\n"
		"                (net_name \"\")\n"
		"                (layer \"B.Cu\")\n"
		"                (uuid \"2d931374-edf7-43e7-92d4-bb3e10af8b39\")\n"
		"                (name \"Upper Pry Zone\")\n"
		"                (hatch edge 0.5)\n"
		"                (connect_pads\n"
		"                        (clearance 0)\n"
		"                )\n"
		"                (min_thickness 0.25)\n"
		"                (filled_areas_thickness no)\n"
		"                (keepout\n"
		"                        (tracks not_allowed)\n"
		"                        (vias not_allowed)\n"
		"                        (pads not_allowed)\n"
		"                        (copperpour allowed)\n"
		"                        (footprints not_allowed)\n"
		"                )\n"
		"                (fill\n"
		"                        (thermal_gap 0.5)\n"
		"                        (thermal_bridge_width 0.5)\n"
		"                )\n"
		"                (polygon\n"
		"                        (pts\n"
		"                                (xy -1.984241 %f) (xy -1.984241 %f) (xy 2.019516 %f) (xy 2.019516 %f)\n"
		"                        )\n"
		"                )\n"
		"        )\n",
		module_height/2 - 0.08 - pry_height,
		module_height/2 - 0.08,
		module_height/2 - 0.08,
		module_height/2 - 0.08 - pry_height		
		);
    
	fprintf(out,
		"	 (zone\n"
		"                (net 0)\n"
		"                (net_name \"\")\n"
		"                (layer \"B.Cu\")\n"
		"                (uuid \"2d931374-edf7-43e7-92d4-bb3e10af8b39\")\n"
		"                (name \"Upper Pry Zone\")\n"
		"                (hatch edge 0.5)\n"
		"                (connect_pads\n"
		"                        (clearance 0)\n"
		"                )\n"
		"                (min_thickness 0.25)\n"
		"                (filled_areas_thickness no)\n"
		"                (keepout\n"
		"                        (tracks not_allowed)\n"
		"                        (vias not_allowed)\n"
		"                        (pads not_allowed)\n"
		"                        (copperpour allowed)\n"
		"                        (footprints not_allowed)\n"
		"                )\n"
		"                (fill\n"
		"                        (thermal_gap 0.5)\n"
		"                        (thermal_bridge_width 0.5)\n"
		"                )\n"
		"                (polygon\n"
		"                        (pts\n"
		"                                (xy -1.984241 %f) (xy -1.984241 %f) (xy 2.019516 %f) (xy 2.019516 %f)\n"
		"                        )\n"
		"                )\n"
		"        )\n",
		-module_height/2 + 0.08 + pry_height,
		-module_height/2 + 0.08,
		-module_height/2 + 0.08,
		-module_height/2 + 0.08 + pry_height		
		);
    
	// Exclusion zone for footprints outside of cut-out on rear
	footprint_exclusion_zone(out,-module_width/2,module_height/2,
				 -co_width/2,-module_height/2);
	footprint_exclusion_zone(out,module_width/2,module_height/2,
				 co_width/2,-module_height/2);
	footprint_exclusion_zone(out,-co_width/2,module_height/2,
				 co_width/2,co_height/2);
	footprint_exclusion_zone(out,-co_width/2,-module_height/2,
				 co_width/2,-co_height/2);
    
    
	// Draw rectangle for component area on rear for cut-out
	fprintf(out,
		"         (fp_rect\n"
		"                (start %.2f %.2f)\n"
		"                (end %.2f %.2f)\n"
		"                (stroke\n"
		"                        (width 0.05)\n"
		"                        (type default)\n"
		"                )\n"
		"                (fill none)\n"
		"                (layer \"B.SilkS\")\n"
		"                (uuid \"a267cc7c-3519-453e-849e-e978b46e7c98\")\n"
		"	   )\n",
		-co_width/2,co_height/2,
		co_width/2,-co_height/2
		);
    
	for(int i=0;i<half_pin_count;i++)
	  {
	    if (pin_present(i+1,pin_mask)) {
	      fprintf(out,
		      "       (pad \"%d\" thru_hole rect\n"
		      "                (at %f %f)\n"
		      "                (size 2.54 2)\n"
		      "                (drill oval 2 1.5)\n"
		      "                (property pad_prop_castellated)\n"
		      "                (layers \"*.Cu\" \"*.Mask\")\n"
		      "                (remove_unused_layers no)\n"
		      "                (thermal_bridge_angle 45)\n"
		      "                (uuid \"f85e55df-ccf1-4a29-ab8b-2b081b1da284\")\n"
		      "        )\n",
		      i+1,
		      -module_width/2,
		      -module_height/2 + 2.54/2.0 + 2.54*i
		      );

	      if (sparepad) 
		fprintf(out,
			"       (pad \"%d\" thru_hole roundrect\n"
			"                (at %f %f 180)\n"
			"                (size 1.25 1.25)\n"
			"                (drill 0.75)\n"
			"                (layers \"*.Cu\" \"*.Mask\" )\n"
			"                (remove_unused_layers no)\n"
			"                (roundrect_rratio 0.2)\n"
			"                (thermal_bridge_angle 45)\n"		  
			"                (uuid \"78a0f889-0c8d-4398-9bd5-55e879a094cf\")\n"
			"       )\n",
			i+1,
			-module_width/2+SPARE_HOLE_DISPLACEMENT,
			-module_height/2 + 2.54/2.0 + 2.54*i
			);
	      
	    }
	    
	  }
	
	for(int i=0;i<half_pin_count;i++)
	  {
	    if (pin_present(i+half_pin_count+1,pin_mask)) {
	      fprintf(out,
		      "       (pad \"%d\" thru_hole rect\n"
		      "                (at %f %f)\n"
		      "                (size 2.54 2)\n"
		      "                (drill oval 2 1.5)\n"
		      "                (property pad_prop_castellated)\n"
		      "                (layers \"*.Cu\" \"*.Mask\")\n"
		      "                (remove_unused_layers no)\n"
		      "                (thermal_bridge_angle 45)\n"
		      "                (uuid \"f85e55df-ccf1-4a29-ab8b-2b081b1da284\")\n"
		      "       )\n",
		      i+1+half_pin_count,
		      module_width/2,
		      -module_height/2 + 2.54/2.0 + 2.54*i
		      );

	      if (sparepad) 
		fprintf(out,
			"       (pad \"%d\" thru_hole circle\n"
			"                (at %f %f 180)\n"
			"                (size 1.25 1.25)\n"
			"                (drill 0.75)\n"
			"                (layers \"*.Cu\" \"*.Mask\" )\n"
			"                (remove_unused_layers no)\n"
			"                (roundrect_rratio 0.2)\n"
			"                (thermal_bridge_angle 45)\n"		  
			"                (uuid \"78a0f889-0c8d-4398-9bd5-55e879a094cf\")\n"
			"       )\n",
			i+1,
			module_width/2-SPARE_HOLE_DISPLACEMENT,
			-module_height/2 + 2.54/2.0 + 2.54*i
			);
	    }
	  }
	
	fprintf(out,")\n");
	fclose(out);
      }
    }
    
    /* ------------------------------------------------------------------------------

       Module Bay footprint (internal to PCB)

       ------------------------------------------------------------------------------ */
    
    for(int edge_type=0;edge_type<2;edge_type++) {
      for(int with_cutout=0;with_cutout<2;with_cutout++) {
	
	snprintf(filename,8192,"%s/MegaCastle.pretty/%s%s%s.kicad_mod",
		 path,
		 bay_footprint_name,
		 with_cutout?"":"-NOCUTOUT",
		 edge_type?"-EDGE":""
		 );
	out = fopen(filename,"w");

	fprintf(stderr,"INFO: Creating %s\n",filename);
    
	fprintf(out,
		"(footprint \"%s\" (version 20221018) (generator pcbnew)\n"
		"  (layer \"F.Cu\")\n"
		"  (attr smd)\n",
		bay_footprint_name
		);
    
	fprintf(out,
		" (fp_text reference \"REF**\" (at %f %f 90 unlocked) (layer \"F.SilkS\")\n"
		"      (effects (font (size 1 1) (thickness 0.1)))\n"
		"    (tstamp 08d1cbd6-6dbd-4696-9477-43e1380128be)\n"
		"  )\n",
		-module_width/2 - 2.54 * 0.8,
		module_height/2
		);
    
	fprintf(out,
		"  (fp_text value \"%s\" (at %f 0 90 unlocked) (layer \"F.Fab\")\n"
		"      (effects (font (size 1 1) (thickness 0.15)))\n"
		"    (tstamp 7848b12f-b698-40ff-9b18-5d3dc1ad63e0)\n"
		"  )\n",
		bay_footprint_name,
		module_width/2 + 2.54* 0.8
		);

	if (!edge_type) {
	  fprintf(out,
		  "  (fp_text user \"VCC Bridge\" (at -3.81 %f unlocked) (layer \"F.SilkS\")\n"
		  "      (effects (font (size 1 1) (thickness 0.1)) (justify left bottom))\n"
		  "    (tstamp c3f3f8b2-4d95-4798-a62c-fde882be7a8f)\n"
		  "  )\n",
		  -(module_height/2 + 4.6)
		  );
	}
    
	fprintf(out,
		"  (fp_text user \"GND Bridge\" (at -4.226381 %f unlocked) (layer \"F.SilkS\")\n"
		"      (effects (font (size 1 1) (thickness 0.1)) (justify left bottom))\n"
		"    (tstamp ececdf77-5944-4d0f-945c-855ee7843a28)\n"
		"  )\n",
		module_height/2 + 4.6 + 1
		);
	fprintf(out,
		"  (fp_text user \"${REFERENCE}\" (at %f %f 90 unlocked) (layer \"F.Fab\")\n"
		"      (effects (font (size 1 1) (thickness 0.15)))\n"
		"    (tstamp 515e6e66-058b-44a5-b9cf-9ec3e273dd1c)\n"
		"  )\n",
		-module_width/2 - 2.54 * 0.8,
		0.0
		);
    
	// VCC and GND bridge silkscreen marks
	if (!edge_type) {
	  draw_line(out,"F.SilkS",
		    -module_width/2,-(module_height/2+2.54*1.75+0),
		    -module_width/2,-(module_height/2+2.54*1.75+1.4),0.1);
	  draw_line(out,"F.SilkS",
		    -module_width/2,-(module_height/2+2.54*1.75+1.4),
		    module_width/2,-(module_height/2+2.54*1.75+1.4),0.1);
	  draw_line(out,"F.SilkS",
		    module_width/2,-(module_height/2+2.54*1.75+0),
		    module_width/2,-(module_height/2+2.54*1.75+1.4),0.1);
	}
    
	draw_line(out,"F.SilkS",
		  -module_width/2,(module_height/2+2.54*1.75+0),
		  -module_width/2,(module_height/2+2.54*1.75+1.4),0.1);
	draw_line(out,"F.SilkS",
		  -module_width/2,(module_height/2+2.54*1.75+1.4),
		  module_width/2,(module_height/2+2.54*1.75+1.4),0.1);
	draw_line(out,"F.SilkS",
		  module_width/2,(module_height/2+2.54*1.75+0),
		  module_width/2,(module_height/2+2.54*1.75+1.4),0.1);
    
	// Module outline silkscreen
	fprintf(out,
		"   (fp_rect (start %f %f) (end %f %f)\n"
		"    (stroke (width 0.1) (type default)) (fill none) (layer \"F.SilkS\") (tstamp 1b929847-1a5f-4d05-bc4e-9961861a32f5))\n",
		-module_width/2,-module_height/2,
		module_width/2,module_height/2
		);
    
	// Draw QR-code style registration boxes
	if (!edge_type) {
	  draw_qr_corner(out,-module_width/2-2.54,-module_height/2,1);
	  draw_qr_corner(out,module_width/2+2.54*2.5,-module_height/2,1);
	} else {
	  draw_qr_corner(out,-module_width/2-2.54,-module_height/2 + 3.81,1);
	  draw_qr_corner(out,module_width/2+2.54*2.5,-module_height/2 + 3.81,1);
	}
	draw_qr_corner(out,-module_width/2-2.54,module_height/2+3.81,0);
	draw_qr_corner(out,module_width/2+2.54*2.5,module_height/2+3.81,1);
    
	// Cut-outs for pry zones and component area
	if (with_cutout)
	  fprintf(out,
		  "     (fp_rect (start %f %f) (end %f %f)\n"
		  "    (stroke (width 0.05) (type default)) (fill none) (layer \"Edge.Cuts\") (tstamp 0442aa54-1eb9-4247-b784-158a997ff791))\n",
		  -co_width/2,-co_height/2,
		  co_width/2,co_height/2);
	if (edge_type) {
	  fprintf(out,
		  "  (fp_rect (start -2 %f) (end 2 %f)\n"
		  "    (stroke (width 0.05) (type default)) (fill none) (layer \"Edge.Cuts\") (tstamp 7fe4220e-eec5-4f8b-b02e-8e7654d86bc8))\n",
		  -module_height/2 + 1.27,
		  -module_height/2
		  );
	} else {
	  fprintf(out,
		  "  (fp_rect (start -2 %f) (end 2 %f)\n"
		  "    (stroke (width 0.05) (type default)) (fill none) (layer \"Edge.Cuts\") (tstamp 7fe4220e-eec5-4f8b-b02e-8e7654d86bc8))\n",
		  -module_height/2 + 1.27,
		  -module_height/2 + 1.27 - 4
		  );
	}
	fprintf(out,
		"  (fp_rect (start -2 %f) (end 2 %f)\n"
		"    (stroke (width 0.05) (type default)) (fill none) (layer \"Edge.Cuts\") (tstamp 47058791-feaf-409b-812c-7a0cb6797c8a))\n",
		module_height/2 - 1.27,
		module_height/2 - 1.27 + 4
	    
		);
    

	if (!edge_type) {
	  fprintf(out,
		  "  (pad \"A1\" smd roundrect (at %f %f) (size 2.54 2.54) (layers \"F.Cu\" \"F.Paste\" \"F.Mask\") (roundrect_rratio 0.25)\n"
		  "    (thermal_bridge_angle 45) (tstamp ae5fa498-358c-4c43-a1ea-5a55b816d453))\n",
		  -module_width/2,-(module_height/2+3)
		  );
	  fprintf(out,
		  "  (pad \"A2\" smd roundrect (at %f %f) (size 2.54 2.54) (layers \"F.Cu\" \"F.Paste\" \"F.Mask\") (roundrect_rratio 0.25)\n"
		  "    (thermal_bridge_angle 45) (tstamp 77e20813-8ea5-4ba4-b0e8-526d94c0eb31))\n",
		  module_width/2,-(module_height/2+3)
		  );
	}
	fprintf(out,
		"  (pad \"B1\" smd roundrect (at %f %f) (size 2.54 2.54) (layers \"F.Cu\" \"F.Paste\" \"F.Mask\") (roundrect_rratio 0.25)\n"
		"    (thermal_bridge_angle 45) (tstamp 91b8277f-56f9-4a55-be1f-442d27436309))\n",
		-module_width/2,(module_height/2+3)
		);
	fprintf(out,
		"  (pad \"B2\" smd roundrect (at %f %f) (size 2.54 2.54) (layers \"F.Cu\" \"F.Paste\" \"F.Mask\") (roundrect_rratio 0.25)\n"
		"    (thermal_bridge_angle 45) (tstamp 83e83792-c3a6-4e3b-a658-764e96850587))\n",
		module_width/2,(module_height/2+3)
		);

	// Draw front courtyard outline
	draw_line(out,"F.CrtYd",
		  -(module_width/2+2.54*1.0),-(module_height/2+2.54*1.75*(1-edge_type)+0),
		  -(module_width/2+2.54*1.0),(module_height/2+2.54*1.75+0),0.05
		  );

	draw_line(out,"F.CrtYd",
		  -(module_width/2+2.54*1.0),(module_height/2+2.54*1.75+0),
		  (module_width/2+2.54*1.0),(module_height/2+2.54*1.75+0),0.05
		  );    
    
	draw_line(out,"F.CrtYd",
		  (module_width/2+2.54*1.0),-(module_height/2+2.54*1.75*(1-edge_type)+0),
		  (module_width/2+2.54*1.0),(module_height/2+2.54*1.75+0),0.05
		  );

	draw_line(out,"F.CrtYd",
		  (module_width/2+2.54*1.0),-(module_height/2+2.54*1.75*(1-edge_type)+0),
		  -(module_width/2+2.54*1.0),-(module_height/2+2.54*1.75*(1-edge_type)+0), 0.05
		  );    
    
    
	for(int i=0;i<half_pin_count;i++)
	  {
	    if (pin_present(i+1,pin_mask))
	      fprintf(out,
		      "       (pad \"%d\" smd roundrect\n"
		      "                (at %f %f)\n"
		      "                (size 2.54 2)\n"
		      "                (layers \"F.Cu\" \"F.Paste\" \"F.Mask\")\n"
		      "                (roundrect_rratio 0.25)\n"
		      "                (thermal_bridge_angle 45) (tstamp 93e18f82-86ef-4410-989e-eb27a6ef3dd8)\n"
		      "        )\n",
		      i+1,
		      -module_width/2,
		      -module_height/2 + 2.54/2.0 + 2.54*i
		      );
	  }
    
	for(int i=0;i<half_pin_count;i++)
	  {
	    if (pin_present(i+half_pin_count+1,pin_mask))
	      fprintf(out,
		      "       (pad \"%d\" smd roundrect\n"
		      "                (at %f %f)\n"
		      "                (size 2.54 2)\n"
		      "                (layers \"F.Cu\" \"F.Paste\" \"F.Mask\")\n"
		      "                (roundrect_rratio 0.25)\n"
		      "                (thermal_bridge_angle 45) (tstamp 93e18f82-86ef-4410-989e-eb27a6ef3dd8)\n"
		      "        )\n",
		      i+1+half_pin_count,
		      module_width/2,
		      -module_height/2 + 2.54/2.0 + 2.54*i
		      );
	  }
    
	fprintf(out,")\n");  
	fclose(out);
      }
    }
  }
    
  /* ---------------------------------------------------------------------------------------------------------

     Now create symbol entities.

     --------------------------------------------------------------------------------------------------------- */
  snprintf(filename,8192,"%s/MegaCastle.kicad_sym",
	   path);
  FILE *in=fopen(filename,"r");
  if (!in) {
    fprintf(stderr,"ERROR: Could not read '%s'\n",filename);
    exit(-1);
  }

  snprintf(filename,8192,"%s/MegaCastle.kicad_sym.tmp",
	   path);
  out=fopen(filename,"w");
  if (!out) {
    fprintf(stderr,"ERROR: Could not write to '%s'\n",filename);
    exit(-1);
  }
  
  char match_module[8192];
  char match_bay[8192];
  snprintf(match_module,8192,"\t(symbol \"%s",footprint_name);
  snprintf(match_bay,8192,"\t(symbol \"%s",bay_footprint_name);
	     
  
  char line[8192];
  int skipping=0;
  line[0]=0; fgets(line,8192,in);
  while(line[0]) {
    if (!strncmp(line,match_module,strlen(match_module))) {
      fprintf(stderr,"INFO: Removing old module symbol.\n");
      skipping=1;
    }
    if (!strncmp(line,match_bay,strlen(match_bay))) {
      skipping=1;
      fprintf(stderr,"INFO: Removing old module symbol.\n");
    }

    if (!strncmp(line,")",1)) {
      fprintf(stderr,"INFO: Found end of library. Preparing to append symbols.\n");
      break;
    }
    
    if (!skipping) fwrite(line,strlen(line),1,out);

    if (!strncmp(line,"\t)",2)) skipping=0;

    line[0]=0; fgets(line,8192,in);
  }
  fclose(in);

  // Output bay symbols
  for(int edge_type=0;edge_type<2;edge_type++) {
    for(int with_cutout=0;with_cutout<2;with_cutout++) {
      
      snprintf(filename,8192,"%s%s%s",
	       bay_footprint_name,
	       with_cutout?"":"-NOCUTOUT",
	       edge_type?"-EDGE":""
	       );

      fprintf(stderr,"INFO: Writing KiCad symbol '%s' with footprint '%s'\n",filename,bay_footprint_name);

      symbol_write(out,filename,bay_footprint_name,1,half_pin_count,pin_mask,edge_type,with_cutout,argv);
      
    }
  }

  // Output module symbols
  for(int edge_type=0;edge_type<2;edge_type++) {
    for(int with_cutout=0;with_cutout<2;with_cutout++) {
      
      snprintf(filename,8192,"%s%s%s",
	       footprint_name,
	       with_cutout?"":"-NOCUTOUT",
	       edge_type?"-EDGE":""
	       );

      fprintf(stderr,"INFO: Writing KiCad symbol '%s' with footprint '%s'\n",filename,footprint_name);

      symbol_write(out,filename,footprint_name,0,half_pin_count,pin_mask,edge_type,with_cutout,argv);
      
    }
  }

  
  
  fprintf(out,")\n");
  fclose(out);

  fprintf(stderr,"INFO: Moving updated symbol library into place.\n");
  char cmd[8192];
  snprintf(cmd,8192,"mv %s/MegaCastle.kicad_sym.tmp %s/MegaCastle.kicad_sym",path,path);
  system(cmd);
  
  return 0;
}

