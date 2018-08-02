/* Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
   AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
   OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
   IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

int iload_file(struct ImageHandle *ih, const char *fname, int rw)
{
	// init image handle structure to zero's
	memset(ih, 0, sizeof(*ih));
	// load file into memory
	if(((ih->d.p)= (void *)load_file(fname,&ih->len)) == 0) return -1;
	return 0;
}

int ifree_file(struct ImageHandle *ih)
{
	if((ih->d.p) != 0) { /*printf("Freeing %d bytes at %p.\n", (int)ih->len, ih->d.p);*/ free(ih->d.p); } else { printf("Nothing to free\n"); }
	memset(ih, 0, sizeof(*ih));
	return 0;
}

/* load a file into memory and return buffer */
uint8_t *load_file(const char *filename, size_t *filelen)
{
	FILE *fp;
	uint8_t *data;
	size_t size,bytesRead;

	/* open file */
	printf("þ Opening '%s' file\n",filename);
	if ((fp = (FILE *)fopen(filename, "rb")) == NULL){ printf("\nCan't open file \"%s\".", filename); return(0); }

	/* get file length */
//	printf("þ Getting length of '%s' file\n",filename);
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if(size <= 0) { printf("Error: Problem with seeking filesize\n"); fclose(fp); return(0); }

	*filelen = size;		/* return size of file to caller */

	/* alloc buffer for file */
//	printf("þ Allocating buffer of %d bytes (%p)\n",(int)size,(void *)size);
	data = (uint8_t *)malloc(size);
	if(data == 0) { printf("\nfailed to allocate memory to load module\n"); fclose(fp); return 0; }

	/* load file into buffer */
//	printf("þ Reading file to buffer\n");
	bytesRead = fread(data, 1, size, fp);

	/* validate it all loaded correctly */
//	printf("þ Validating size correct %d=%d\n",(int)bytesRead,(int)size);
	if(bytesRead != size) { printf("\nfailed to load module into buffer\n"); free(data); fclose(fp); return 0; }

	/* close the file */
//	printf("þ Closing file\n\n");
	fclose(fp);
	return(data);
}

/* load a file into memory and return buffer */
int save_file(const char *filename, const uint8_t *filebuf, size_t filelen)
{
	FILE *fp;
	size_t bytesWritten;

	/* open file */
//	printf("þ Opening '%s' file for writing\n",filename);
	if ((fp = (FILE *)fopen(filename, "wb")) == NULL){ printf("\nCan't open file \"%s\".", filename); return(-1); }

	/* load file into buffer */
//	printf("þ Writing to file\n");
	bytesWritten = fwrite((void *)filebuf, (size_t)1, (size_t)filelen, fp);

	/* validate it all loaded correctly */
//	printf("þ Validating size correct %d=%d\n",(int)bytesWritten,(int)filelen);
	if(bytesWritten != filelen) { printf("\nfailed to write buffer\n"); fclose(fp); return(-2); }

	/* close the file */
//	printf("þ All OK, closing file\n\n");
	fclose(fp);

	return(0);
}

static int memcmp_mask(const void *ptr1, const void *ptr2, const void *mask, size_t len)
{
    const uint8_t *p1 = (const uint8_t*)ptr1;
    const uint8_t *p2 = (const uint8_t*)ptr2;
    const uint8_t *m = (const uint8_t*)mask;

    while(len--)
    {
	int diff = m?(*p2 & *m)-(*p1 & *m):*p2-*p1;
	if (diff) return diff>0?1:-1;
	p1++;
	p2++;
	if (m) m++;
    }
    return 0;
}

unsigned char *search(ImageHandle *fh, unsigned char *pNeedle, unsigned char *pMask, int needle_len, int offset)
{
	int i,j;
	int search_result;
	unsigned char *start_adr;
	unsigned int start_len;
			
	/* lets now find patterns.. */
	for(i=offset; i+needle_len < (fh->len)-offset; )
	{
		// find a match within the file start/end boundaries from given offset i ..
		if((search_result = search_image(fh, i, pNeedle, pMask, needle_len, 2)) == -1) break; 

		// found a match.. lets return it to caller
		if ( i+needle_len < fh->len)
		{
//			printf("\nFound match bytes : 0x%lx len=%d ", (long)search_result, (int)needle_len);
			if(search_result != 0) {
				return(start_adr = (fh->d.p)+search_result);
			}
		}
		// skip past the last searched point and carry on trying to find matches in entire buffer..
		i += search_result + needle_len;
	}
	printf("No match found");	
	return 0;
}

int search_replace(ImageHandle *fh, unsigned char *pNeedle, unsigned char *pMask, unsigned char *pNeedle_patch, unsigned char *pMask_patch, int needle_len)
{
	int i,j, found=0;
	int search_result;
	unsigned char *start_adr;
	unsigned int start_len;
	unsigned int patched_bytes;
			
	/* lets now find patterns.. */
	patched_bytes = 0;
	for(i=0; i+needle_len < fh->len; )
	{
		// find all matches..
		if((search_result = search_image(fh, i, pNeedle, pMask, needle_len, 2)) == -1) break; 

		// found a match.. lets substitute it
		if ( i+needle_len < fh->len)
		{
//			printf("Found match bytes : 0x%lx len=%d, (Match #%d) { ", (long)search_result, (int)needle_len, found);
			if(search_result != 0) {
							
				start_adr = (fh->d.p)+search_result;
				start_len = needle_len;
//				printf("\n*** before [original] ***\n");
//				hexdump( start_adr, start_len, " }\n");
//				printf("*** patching ***\n");
				for(j=0,patched_bytes=0;j < start_len; j++)
				{
						if(pMask_patch[j] == 0xff) {
							start_adr[j] = pNeedle_patch[j];	// patch original source byte
							patched_bytes++;
						}
				}

//				printf("*** after [%d bytes patched] ***\n", patched_bytes);
//				hexdump( start_adr, start_len, " }\n");

			}
			printf("\n");
			found++;
		}
					
		// skip past the last searched point and carry on trying to find matches in entire buffer..
		i += search_result + needle_len;
	}
	return patched_bytes;
}


/* returns -1 on failure, start if found, start+align if not found */
int search_image(const struct ImageHandle *ih, int start, const void *needle, const void *mask, int len, int align)
{
    if (start<0) return -1;

    for (;start+len<ih->len;start+=align)
    {
		if(memcmp_mask(ih->d.u8+start, needle, mask, len)==0)
		{
//			printf("got one at 0x%x\n", start);
			return start;
		}
    }
//    printf("failed, returning 0x%x\n", start);
    return -1;
}

#if 1
void hexdump(uint8_t *buf, int len, const char *end)
{
    while(len--) printf("%02x%s", *buf++, len?" ":"");
    printf("%s", end);
}
#endif

void hexdump_le_table(uint8_t *buf, int len, const char *end)
{
	int i,j=0,k=0;
	int val;

	for(i=0;i<len;i++)
	{
//		val = *(buf++);
		
		val = get16(buf);
		buf++;
		buf++;
		
		printf("0x%-4.4x",val);
		k++;

		if(k==1)
		{
			if(i < len-1)
			{
				printf(",");
			}
			
			k=0;
		}

		if(j==15)
		{
			printf("\n");
			j=0;
		}
		else
		{
			j++;
		}

	}
    printf("%s", end);
}

//This function compares text strings, one of which can have wildcards ('*').
//
char matchString(char * test, char * pWildText, char bCaseSensitive)  // By default, match on 'X' vs 'x'
{
        char cAltTerminator = '\0'; // For function names, for example, you can stop at the first '('
        char bMatch = 1;
        char * pAfterLastWild = 0; // The location after the last '*', if we’ve encountered one
        char * pAfterLastTame = 0; // The location in the tame string, from which we started after last wildcard
        char t, w;
 
        // Walk the text strings one character at a time.
        while (1)
        {
                t = *test;
                w = *pWildText;
 
                // How do you match a unique text string?
                if (!t || t == cAltTerminator)
                {
                        // Easy: unique up on it!
                        if (!w || w == cAltTerminator)
                        {
                                break;                                   // "x" matches "x"
                        }
                        else if (w == '*')
                        {
                                pWildText++;
                                continue;                           // "x*" matches "x" or "xy"
                        }
                        else if (pAfterLastTame)
                        {
                                if (!(*pAfterLastTame) || *pAfterLastTame == cAltTerminator)
                                {
                                        bMatch = 0;
                                        break;
                                }
                                test = pAfterLastTame++;
                                pWildText = pAfterLastWild;
                                continue;
                        }
 
                        bMatch = 0;
                        break;                                           // "x" doesn't match "xy"
                }
                else
                {
                        if (!bCaseSensitive)
                        {
                                // Lowercase the characters to be compared.
                                if (t >= 'A' && t <= 'Z')
                                {
                                        t += ('a' - 'A');
                                }
 
                                if (w >= 'A' && w <= 'Z')
                                {
                                        w += ('a' - 'A');
                                }
                        }
 
                        // How do you match a tame text string?
                        if (t != w)
                        {
                                // The tame way: unique up on it!
                                if (w == '*')
                                {
                                        pAfterLastWild = ++pWildText;
                                        pAfterLastTame = test;
                                        w = *pWildText;
 
                                        if (!w || w == cAltTerminator)
                                        {
                                                break;                           // "*" matches "x"
                                        }
                                        continue;                           // "*y" matches "xy"
                                }
                                else if (pAfterLastWild)
                                {
                                        if (pAfterLastWild != pWildText)
                                        {
                                                pWildText = pAfterLastWild;
                                                w = *pWildText;
                                                
                                                if (!bCaseSensitive && w >= 'A' && w <= 'Z')
                                                {
                                                        w += ('a' - 'A');
                                                }
 
                                                if (t == w)
                                                {
                                                        pWildText++;
                                                }
                                        }
                                        test++;
                                        continue;                           // "*sip*" matches "mississippi"
                                }
                                else
                                {
                                        bMatch = 0;
                                        break;                                   // "x" doesn't match "y"
                                }
                        }
                }
 
                test++;
                pWildText++;
        }
 
        return bMatch;
}

unsigned short get16(unsigned char *s) { return (unsigned short)( ((s[1] <<  8)) | ((s[0]                                   )) ); }
unsigned long  get32(unsigned char *s) { return (unsigned long )( ((s[3] << 24)) | ((s[2] <<  16)) |((s[1] <<  8)) |  ((s[0])) ); }
