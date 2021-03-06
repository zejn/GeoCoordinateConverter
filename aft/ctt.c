// GK - Converter between Gauss-Krueger/TM and WGS84 coordinates for Slovenia
// Copyright (c) 2014-2015 Matjaz Rihtar <matjaz@eunet.si>
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 2.1 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see http://www.gnu.org/licenses/
//
// ctt.c: Program for pre-calculating affine transformation tables
//
#include "common.h"
#include "geo.h"

#define SW_VERSION T("2.05")
#define SW_BUILD   T("Nov 28, 2015")

typedef struct triang {
  int t1, t2, t3;
} TRIANG;

GEOUTM *gk, *tm;
TRIANG *tri;
AFT *aft;

// global variables
TCHAR *prog;  // program name
int debug;


// ----------------------------------------------------------------------------
// solve
// ----------------------------------------------------------------------------
int solve(AFT *aft)
{
  int ii, jj, kk, nv;
  double m67[6][7]; 

  nv = 6;  // number of unknowns

  // create augmented matrix for affine transformation
  m67[0][0] = aft->src[0].x;
  m67[0][1] = aft->src[0].y;
  m67[0][2] = 1.0;
  m67[0][3] = m67[0][4] = m67[0][5] = 0.0;
  m67[0][6] = aft->dst[0].x;

  m67[1][0] = aft->src[1].x;
  m67[1][1] = aft->src[1].y;
  m67[1][2] = 1.0;
  m67[1][3] = m67[1][4] = m67[1][5] = 0.0;
  m67[1][6] = aft->dst[1].x;

  m67[2][0] = aft->src[2].x;
  m67[2][1] = aft->src[2].y;
  m67[2][2] = 1.0;
  m67[2][3] = m67[2][4] = m67[2][5] = 0.0;
  m67[2][6] = aft->dst[2].x;

  m67[3][0] = m67[3][1] = m67[3][2] = 0.0;
  m67[3][3] = aft->src[0].x;
  m67[3][4] = aft->src[0].y;
  m67[3][5] = 1.0;
  m67[3][6] = aft->dst[0].y;

  m67[4][0] = m67[4][1] = m67[4][2] = 0.0;
  m67[4][3] = aft->src[1].x;
  m67[4][4] = aft->src[1].y;
  m67[4][5] = 1.0;
  m67[4][6] = aft->dst[1].y;

  m67[5][0] = m67[5][1] = m67[5][2] = 0.0;
  m67[5][3] = aft->src[2].x;
  m67[5][4] = aft->src[2].y;
  m67[5][5] = 1.0;
  m67[5][6] = aft->dst[2].y;

  // row-reduce augmented matrix
  for (ii = 0; ii < nv; ii++) {
    if (m67[ii][ii] == 0) {
      for (jj = ii+1; jj < nv; jj++) {
        if (m67[jj][ii] != 0) {
          for (kk = ii; kk < nv+1; kk++)
            m67[ii][kk] += m67[jj][kk];
          break;
        }
      }
      if (jj == nv) {
        fprintf(stderr, T("Repeated equation - can't solve!\n"));
        exit(10);
      }
    }

    for (kk = nv; kk >= ii; kk--)
      m67[ii][kk] /= m67[ii][ii];

    for (jj = ii+1; jj < nv; jj++) {
      for (kk = nv; kk >= ii; kk--)
        m67[jj][kk] -= m67[ii][kk] * m67[jj][ii];
    }
  }

  // make unit matrix on the left
  for (ii = nv-1; ii > 0; ii--) {
    for (jj = ii-1; jj >= 0; jj--) {
      m67[jj][nv] -= m67[jj][ii] * m67[ii][nv];
      m67[jj][ii] = 0;
    }
  }

  // results are in the last column
  aft->a = m67[0][nv];
  aft->b = m67[1][nv];
  aft->c = m67[2][nv];
  aft->d = m67[3][nv];
  aft->e = m67[4][nv];
  aft->f = m67[5][nv];

  return 0;
} /* solve */


// ----------------------------------------------------------------------------
// centroid
// ----------------------------------------------------------------------------
void centroid_xy(AFT *aft)
{
  double cx, cy;

  // src centroid
  cx = (aft->src[0].x + aft->src[1].x + aft->src[2].x) / 3.0;
  cy = (aft->src[0].y + aft->src[1].y + aft->src[2].y) / 3.0;

  aft->cxy = cx * cy;
} /* centroid_xy */


// ----------------------------------------------------------------------------
// quick_sort
// ----------------------------------------------------------------------------
void quick_sort(AFT *aft, int n)
{
  int ii, jj;
  double pv;
  AFT tmp;

  if (n < 2) return;

  pv = aft[n / 2].cxy;
  for (ii = 0, jj = n - 1; ; ii++, jj--) {
    while (aft[ii].cxy < pv) ii++;

    while (pv < aft[jj].cxy) jj--;

    if (ii >= jj) break;

    tmp = aft[ii];
    aft[ii] = aft[jj];
    aft[jj] = tmp;
  }

  quick_sort(aft, ii);
  quick_sort(aft + ii, n - ii);
} /* quick_sort */


// ----------------------------------------------------------------------------
// print_header
// ----------------------------------------------------------------------------
void print_header(FILE *out, TCHAR *outname)
{
  fprintf(out, T("// GK - Converter between Gauss-Krueger/TM and WGS84 coordinates for Slovenia\n"));
  fprintf(out, T("// Copyright (c) 2014-2015 Matjaz Rihtar <matjaz@eunet.si>\n"));
  fprintf(out, T("// All rights reserved.\n"));
  fprintf(out, T("//\n"));
  fprintf(out, T("// This program is free software: you can redistribute it and/or modify\n"));
  fprintf(out, T("// it under the terms of the GNU Lesser General Public License as published\n"));
  fprintf(out, T("// by the Free Software Foundation, either version 2.1 of the License, or\n"));
  fprintf(out, T("// (at your option) any later version.\n"));
  fprintf(out, T("//\n"));
  fprintf(out, T("// This program is distributed in the hope that it will be useful,\n"));
  fprintf(out, T("// but WITHOUT ANY WARRANTY; without even the implied warranty of\n"));
  fprintf(out, T("// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"));
  fprintf(out, T("// GNU Lesser General Public License for more details.\n"));
  fprintf(out, T("//\n"));
  fprintf(out, T("// You should have received a copy of the GNU Lesser General Public License\n"));
  fprintf(out, T("// along with this program; if not, see http://www.gnu.org/licenses/\n"));
  fprintf(out, T("//\n"));
  if (ferror(out)) {
    fprintf(stderr, T("%s: %s\n"), outname, xstrerror()); exit(2);
  }
  fflush(out);
} /* print_header */


// ----------------------------------------------------------------------------
// usage
// ----------------------------------------------------------------------------
void usage(TCHAR *prog, int ver_only)
{
  fprintf(stderr, T("%s %s  Copyright (c) 2014-2015 Matjaz Rihtar  (%s)\n"),
          prog, SW_VERSION, SW_BUILD);
  if (ver_only) return;
  fprintf(stderr, T("Usage: %s [<options>] <gknodename> <tmnodename> <elename>\n"), prog);
  fprintf(stderr, T("  -d            enable debug output\n"));
  fprintf(stderr, T("  <gknodename>  file with nodes in GK\n"));
  fprintf(stderr, T("  <tmnodename>  file with nodes in TM\n"));
  fprintf(stderr, T("  <elename>     file with triangle elements\n"));
} /* usage */


// ----------------------------------------------------------------------------
// main (mingw32 unicode wrapper)
// ----------------------------------------------------------------------------
#if defined(__MINGW32__) && defined(_WCHAR)
extern int _CRT_glob;
extern
#ifdef __cplusplus
"C"
#endif
void __wgetmainargs(int*, TCHAR***, TCHAR***, int, int*);
int tmain(int argc, TCHAR *argv[]);

int main() {
  TCHAR **argv, **enpv;
  int argc, si = 0;

  __wgetmainargs(&argc, &argv, &enpv, _CRT_glob, &si);
  return tmain(argc, argv);
}
#endif


// ----------------------------------------------------------------------------
// tmain
// ----------------------------------------------------------------------------
int tmain(int argc, TCHAR *argv[])
{
  int ii, ac, opt;
  TCHAR *s, *av[MAXC];
  TCHAR gknodename[MAXS+1], tmnodename[MAXS+1], elename[MAXS+1];
  TCHAR outname[MAXS+1];
  FILE *gknode, *tmnode, *ele, *out;
  TCHAR line[MAXS+1], col1[MAXS+1];
  int ln, lnc, n;
  int gksize, tmsize, trisize;
  double x, y, xs, ys, xt, yt;
  int maxt, t1, t2, t3;

  // Get program name
  if ((prog = strrchr(argv[0], DIRSEP)) == NULL) prog = argv[0];
  else prog++;
  if ((s = strstr(prog, T(".exe"))) != NULL) *s = T('\0');
  if ((s = strstr(prog, T(".EXE"))) != NULL) *s = T('\0');

  // Default global flags
  debug = 0;   // no debug

  // Parse command line
  ac = 0; opt = 1;
  for (ii = 1; ii < argc && ac < MAXC; ii++) {
    if (opt && *argv[ii] == T('-')) {
      if (strcasecmp(argv[ii], T("--")) == 0) { // end of options
        opt = 0;
        continue;
      }
      s = argv[ii];
      while (*++s) {
        switch (*s) {
          case T('d'): // debug
            debug++;
            break;
          case T('v'): // version
            usage(prog, 1); // show version only
            exit(0);
            break;
          default: // usage
usage:      usage(prog, 0);
            exit(1);
        }
      }
      continue;
    } // if opt
    av[ac] = (TCHAR *)malloc(MAXS+1);
    if (av[ac] == NULL) {
      fprintf(stderr, T("malloc(av): %s\n"), xstrerror()); exit(3);
    }
    xstrncpy(av[ac++], argv[ii], MAXS);
  } // for each argc
  av[ac] = NULL;

  if (ac < 3) goto usage;

  xstrncpy(gknodename, av[0], MAXS);
  xstrncpy(tmnodename, av[1], MAXS);
  xstrncpy(elename, av[2], MAXS);

  gknode = fopen(gknodename, T("r"));
  if (gknode == NULL) {
    fprintf(stderr, T("%s: %s\n"), gknodename, xstrerror()); exit(2);
  }

  tmnode = fopen(tmnodename, T("r"));
  if (tmnode == NULL) {
    fprintf(stderr, T("%s: %s\n"), tmnodename, xstrerror()); exit(2);
  }

  ele = fopen(elename, T("r"));
  if (ele == NULL) {
    fprintf(stderr, T("%s: %s\n"), elename, xstrerror()); exit(2);
  }

  // parse GK node list
  gksize = -1;
  for (ln = lnc = 0; ; ) {
    // Read (next) line
    s = fgets(line, MAXS, gknode);
    if (ferror(gknode)) {
      fprintf(stderr, T("%s: %s\n"), gknodename, xstrerror());
      break;
    }
    if (feof(gknode)) break;
    ln++;

    s = xstrtrim(line);
    if (s[0] == T('#')) continue;  // ignore comments
    lnc++;

    if (lnc == 1) {  // parse header
      n = sscanf(s, T("%d"), &gksize);
      if (n != 1) {
        fprintf(stderr, T("%s: parse error at line %d\n"), gknodename, ln);
        continue;
      }
      if (gksize <= 0) {
        fprintf(stderr, T("%s: strange number of nodes in header\n"), gknodename);
        break;
      }
      gk = malloc(gksize*sizeof(GEOUTM));
      if (gk == NULL) {
        fprintf(stderr, T("malloc(gk): %s\n"), xstrerror()); exit(3);
      }
      memset(gk, 0, gksize*sizeof(GEOUTM));
      if (debug)
        fprintf(stderr, T("%s: %d nodes\n"), gknodename, gksize);
      continue;
    }

    n = sscanf(s, T("%10240s %lf %lf"), col1, &y, &x);
    if (n != 3) {
      fprintf(stderr, T("%s: parse error at line %d\n"), gknodename, ln);
      continue;
    }
    gk[lnc-2].x = x;
    gk[lnc-2].y = y;
  } // parse GK node list
  fclose(gknode);

  // parse TM node list
  tmsize = -1;
  for (ln = lnc = 0; ; ) {
    // Read (next) line
    s = fgets(line, MAXS, tmnode);
    if (ferror(tmnode)) {
      fprintf(stderr, T("%s: %s\n"), tmnodename, xstrerror());
      break;
    }
    if (feof(tmnode)) break;
    ln++;

    s = xstrtrim(line);
    if (s[0] == T('#')) continue;  // ignore comments
    lnc++;

    if (lnc == 1) {  // parse header
      n = sscanf(s, T("%d"), &tmsize);
      if (n != 1) {
        fprintf(stderr, T("%s: parse error at line %d\n"), tmnodename, ln);
        continue;
      }
      if (tmsize <= 0) {
        fprintf(stderr, T("%s: strange number of nodes in header\n"), tmnodename);
        break;
      }
      if (tmsize != gksize) {
        fprintf(stderr, T("%s: number of nodes differ from GK\n"), tmnodename);
      }
      tm = malloc(tmsize*sizeof(GEOUTM));
      if (tm == NULL) {
        fprintf(stderr, T("malloc(tm): %s\n"), xstrerror()); exit(3);
      }
      memset(tm, 0, tmsize*sizeof(GEOUTM));
      if (debug)
        fprintf(stderr, T("%s: %d nodes\n"), tmnodename, tmsize);
      continue;
    }

    n = sscanf(s, T("%10240s %lf %lf"), col1, &y, &x);
    if (n != 3) {
      fprintf(stderr, T("%s: parse error at line %d\n"), tmnodename, ln);
      continue;
    }
    tm[lnc-2].x = x;
    tm[lnc-2].y = y;
  } // parse TM node list
  fclose(tmnode);

  // parse ELE list
  trisize = -1; maxt = -1;
  for (ln = lnc = 0; ; ) {
    // Read (next) line
    s = fgets(line, MAXS, ele);
    if (ferror(ele)) {
      fprintf(stderr, T("%s: %s\n"), elename, xstrerror());
      break;
    }
    if (feof(ele)) break;
    ln++;

    s = xstrtrim(line);
    if (s[0] == T('#')) continue;  // ignore comments
    lnc++;

    if (lnc == 1) {  // parse header
      n = sscanf(s, T("%d"), &trisize);
      if (n != 1) {
        fprintf(stderr, T("%s: parse error at line %d\n"), elename, ln);
        continue;
      }
      if (trisize <= 0) {
        fprintf(stderr, T("%s: strange number of triangles in header\n"), elename);
        break;
      }
      tri = malloc(trisize*sizeof(GEOUTM));
      if (tri == NULL) {
        fprintf(stderr, T("malloc(tri): %s\n"), xstrerror()); exit(3);
      }
      memset(tri, 0, trisize*sizeof(GEOUTM));
      if (debug)
        fprintf(stderr, T("%s: %d triangles\n"), elename, trisize);
      continue;
    }

    n = sscanf(s, T("%10240s %d %d %d"), col1, &t1, &t2, &t3);
    if (n != 4) {
      fprintf(stderr, T("%s: parse error at line %d\n"), elename, ln);
      continue;
    }
    tri[lnc-2].t1 = t1;
    tri[lnc-2].t2 = t2;
    tri[lnc-2].t3 = t3;

    if (t1 > maxt) maxt = t1;
    if (t2 > maxt) maxt = t2;
    if (t3 > maxt) maxt = t3;
  } // parse ELE list
  fclose(ele);

  if (maxt > gksize || maxt > tmsize) {
    fprintf(stderr, T("%s: referenced unknown node numbers\n"), elename);
  }

  aft = malloc(trisize*sizeof(AFT));
  if (aft == NULL) {
    fprintf(stderr, T("malloc(aft): %s\n"), xstrerror()); exit(3);
  }

  // prepare GK-->TM AFT table
  memset(aft, 0, trisize*sizeof(AFT));
  for (ii = 0; ii < trisize; ii++) {
    t1 = tri[ii].t1;
    t2 = tri[ii].t2;
    t3 = tri[ii].t3;

    aft[ii].src[0] = gk[t1];
    aft[ii].src[1] = gk[t2];
    aft[ii].src[2] = gk[t3];

    aft[ii].dst[0] = tm[t1];
    aft[ii].dst[1] = tm[t2];
    aft[ii].dst[2] = tm[t3];

    solve(&aft[ii]);

    centroid_xy(&aft[ii]);
  }
  quick_sort(aft, trisize); // sort by src centroid x*y

  // write out GK-->TM AFT table
  xstrncpy(outname, T("aft_gktm.h"), MAXS);
  out = fopen(outname, T("w"));
  if (out == NULL) {
    fprintf(stderr, T("%s: %s\n"), outname, xstrerror()); exit(2);
  }
  print_header(out, outname);
  fprintf(out, T("// %s: Affine transformation table from GK to TM for Slovenia\n"), outname);
  fprintf(out, T("//\n"));
  fprintf(out, T("AFT aft_gktm[%d] = {\n"), trisize);
  if (ferror(out)) {
    fprintf(stderr, T("%s: %s\n"), outname, xstrerror()); exit(2);
  }
  for (ii = 0; ii < trisize; ) {
    fprintf(out, T("{"));
    fprintf(out, T("{{%.3f,%.3f},{%.3f,%.3f},{%.3f,%.3f}},"),
      aft[ii].src[0].x, aft[ii].src[0].y,
      aft[ii].src[1].x, aft[ii].src[1].y,
      aft[ii].src[2].x, aft[ii].src[2].y);
    fprintf(out, T("{{%.3f,%.3f},{%.3f,%.3f},{%.3f,%.3f}},"),
      aft[ii].dst[0].x, aft[ii].dst[0].y,
      aft[ii].dst[1].x, aft[ii].dst[1].y,
      aft[ii].dst[2].x, aft[ii].dst[2].y);
    fprintf(out, T("%.1f,%.14f,%.14f,%.14f,%.14f,%.14f,%.14f"),
      aft[ii].cxy,
      aft[ii].a, aft[ii].b, aft[ii].c,
      aft[ii].d, aft[ii].e, aft[ii].f);

    ii++;
    if (ii == trisize) fprintf(out, T("}\n"));
    else fprintf(out, T("},\n"));

    if (ferror(out)) {
      fprintf(stderr, T("%s: %s\n"), outname, xstrerror()); exit(2);
    }
  }
  fprintf(out, T("};\n"));
  if (ferror(out)) {
    fprintf(stderr, T("%s: %s\n"), outname, xstrerror()); exit(2);
  }
  fclose(out);
  if (debug)
    fprintf(stderr, T("Created %s\n"), outname);

  // prepare TM-->GK AFT table
  memset(aft, 0, trisize*sizeof(AFT));
  for (ii = 0; ii < trisize; ii++) {
    t1 = tri[ii].t1;
    t2 = tri[ii].t2;
    t3 = tri[ii].t3;

    aft[ii].src[0] = tm[t1];
    aft[ii].src[1] = tm[t2];
    aft[ii].src[2] = tm[t3];

    aft[ii].dst[0] = gk[t1];
    aft[ii].dst[1] = gk[t2];
    aft[ii].dst[2] = gk[t3];

    solve(&aft[ii]);

    centroid_xy(&aft[ii]);
  }
  quick_sort(aft, trisize); // sort by src centroid x*y

  // write out TM-->GK AFT table
  xstrncpy(outname, T("aft_tmgk.h"), MAXS);
  out = fopen(outname, T("w"));
  if (out == NULL) {
    fprintf(stderr, T("%s: %s\n"), outname, xstrerror()); exit(2);
  }
  print_header(out, outname);
  fprintf(out, T("// %s: Affine transformation table from TM to GK for Slovenia\n"), outname);
  fprintf(out, T("//\n"));
  fprintf(out, T("AFT aft_tmgk[%d] = {\n"), trisize);
  if (ferror(out)) {
    fprintf(stderr, T("%s: %s\n"), outname, xstrerror()); exit(2);
  }
  for (ii = 0; ii < trisize; ) {
    fprintf(out, T("{"));
    fprintf(out, T("{{%.3f,%.3f},{%.3f,%.3f},{%.3f,%.3f}},"),
      aft[ii].src[0].x, aft[ii].src[0].y,
      aft[ii].src[1].x, aft[ii].src[1].y,
      aft[ii].src[2].x, aft[ii].src[2].y);
    fprintf(out, T("{{%.3f,%.3f},{%.3f,%.3f},{%.3f,%.3f}},"),
      aft[ii].dst[0].x, aft[ii].dst[0].y,
      aft[ii].dst[1].x, aft[ii].dst[1].y,
      aft[ii].dst[2].x, aft[ii].dst[2].y);
    fprintf(out, T("%.1f,%.14f,%.14f,%.14f,%.14f,%.14f,%.14f"),
      aft[ii].cxy,
      aft[ii].a, aft[ii].b, aft[ii].c,
      aft[ii].d, aft[ii].e, aft[ii].f);

    ii++;
    if (ii == trisize) fprintf(out, T("}\n"));
    else fprintf(out, T("},\n"));

    if (ferror(out)) {
      fprintf(stderr, T("%s: %s\n"), outname, xstrerror()); exit(2);
    }
  }
  fprintf(out, T("};\n"));
  if (ferror(out)) {
    fprintf(stderr, T("%s: %s\n"), outname, xstrerror()); exit(2);
  }
  fclose(out);
  if (debug)
    fprintf(stderr, T("Created %s\n"), outname);

  return 0;
} /* main */
