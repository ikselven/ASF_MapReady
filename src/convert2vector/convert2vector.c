#include "asf.h"
#include "asf_meta.h"
#include "asf_vector.h"
#include "asf_endian.h"
#include <stdio.h>
#include <ctype.h>

#define VERSION 1.0

static
void usage(char *name)
{
 printf("\n"
	"USAGE:\n"
	"   %s [-list] <input format> <output format> <input file> <output file>\n",
	name);
 printf("\n"
	"REQUIRED ARGUMENTS:\n"
	"    input format:  Input format: point, polygon, meta, leader\n"
	"    output format: Output format: shape, kml\n"
	"    input file:    Name of the input file\n"
	"    output file:   Basename of the output file.\n");
 printf("\n"
	"OPTIONAL ARGUMENTS:\n"
	"   -list:          Input file name contains a list of filenames to be "
	"converted.\n");
 printf("\n"
	"DESCRIPTION:\n"
	"   Converts metadata information into external vector formats\n");
 printf("\n"
	"Version: %.2f, ASF InSAR Tools\n"
	"\n",VERSION);
 exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
  meta_parameters *meta;
  char informat[25], outformat[25], infile[255], outfile[255];
  extern int currArg; /* pre-initialized to 1; like optind */
  int listflag=0;
  
  /* parse command line */
  while (currArg < (argc-4)) {
    char *key = argv[currArg++];
    if (strmatch(key,"-list")) {
      listflag=1;
    }
    else {
      printf( "\n**Invalid option:  %s\n",argv[currArg-1]); 
      usage(argv[0]);
    }
  }
  if ((argc-currArg) < 4) {
    printf("Insufficient arguments.\n"); 
    usage(argv[0]);
  }
  
  sprintf(informat, "%s", argv[currArg]);
  sprintf(outformat, "%s", argv[currArg+1]);
  sprintf(infile, "%s", argv[currArg+2]);
  sprintf(outfile, "%s", argv[currArg+3]);
  
  asfSplashScreen (argc, argv);


  // Call library functions that get the work done
  if (listflag) {
    if (strcmp(uc(informat), "META")==0 && strcmp(uc(outformat), "SHAPE")==0) {
      asfPrintStatus("   Converting list of metadata files into a shape file"
		     " ...\n\n");
      meta2shape_list(infile, outfile);
    }
    else if (strcmp(uc(informat), "META")==0 && strcmp(uc(outformat), "KML")==0) {
      asfPrintStatus("   Converting list of metadata files into a kml file"
		     " ...\n\n");
      meta2kml_list(infile, outfile);
    }
    else if 
      (strcmp(uc(informat), "LEADER")==0 && strcmp(uc(outformat), "SHAPE")==0) {
      asfPrintStatus("   Converting list of leader files into a shape file"
		     " ...\n\n");
      leader2shape_list(infile, outfile);
    }
    else if 
      (strcmp(uc(informat), "LEADER")==0 && strcmp(uc(outformat), "KML")==0) {
      asfPrintStatus("   Converting list of leader files into a kml file"
		     " ...\n\n");
      leader2kml_list(infile, outfile);
    }
    else if 
      (strcmp(uc(informat), "POINT")==0 && strcmp(uc(outformat), "SHAPE")==0) {
      asfPrintStatus("   Converting a point file into a shape file ...\n\n");
      write_polygon_shapefile(outfile, infile, "");
    }
    else if 
      (strcmp(uc(informat), "POLYGON")==0 && strcmp(uc(outformat), "SHAPE")==0) {
      asfPrintStatus("   Converting a point file into a shape file ...\n\n");
      write_polygon_shapefile(outfile, infile, "");
    }
    else if 
      (strcmp(uc(informat), "POINT")==0 && strcmp(uc(outformat), "KML")==0) {
      asfPrintStatus("   Converting list of points into a kml file ...\n\n");
      point2kml_list(infile, outfile);
    }
    else if 
      (strcmp(uc(informat), "POLYGON")==0 && strcmp(uc(outformat), "KML")==0) {
      asfPrintStatus("   Converting list of points into a kml file ...\n\n");
      polygon2kml_list(infile, outfile);
    }
    else
      asfPrintStatus("   Unsupported conversion\n\n");
  }
  else {
    if (strcmp(uc(informat), "META")==0 && strcmp(uc(outformat), "SHAPE")==0) {
      asfPrintStatus("   Converting a metadata file into a shape file ...\n\n");
      meta2shape(infile, outfile);
    }
    else if (strcmp(uc(informat), "META")==0 && strcmp(uc(outformat), "KML")==0) {
      asfPrintStatus("   Converting a metadata file into a kml file ...\n\n");
      meta = meta_read(infile);
      meta2kml(outfile, meta);
    }
    else if 
      (strcmp(uc(informat), "LEADER")==0 && strcmp(uc(outformat), "SHAPE")==0) {
      asfPrintStatus("   Converting a leader file into a shape file ...\n\n");
      leader2shape(infile, outfile);
    }
    else if 
      (strcmp(uc(informat), "LEADER")==0 && strcmp(uc(outformat), "KML")==0) {
      asfPrintStatus("   Converting a leader file into a kml file ...\n\n");
      leader2kml(infile, outfile);
    }
    else if 
      (strcmp(uc(informat), "SHAPE")==0 && strcmp(uc(outformat), "POINT")==0) {
      asfPrintStatus("   Converting a shape file into a point file ...\n\n");
      read_shapefile(infile, outfile);
    }
    else if 
      (strcmp(uc(informat), "POINT")==0 && strcmp(uc(outformat), "SHAPE")==0) {
      asfPrintStatus("   Converting list of points into a shape file ...\n\n");
      write_point_shapefile(outfile, infile);
    }
    else 
      asfPrintStatus("   Unsupported conversion\n\n");
  }

  return(0);
}

