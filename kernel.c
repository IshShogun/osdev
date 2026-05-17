#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "multiboot2.h"

/* Check if the compiler thinks you are targeting the wrong operating system. */
/* The osdev wiki assumes you're running linux, but macos would be __APPLE__, if its windows who f*cking cares
	 it doesnt deserve to run anyway */

#define PIXEL uint32_t   /* pixel pointer */

#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

struct PSF1_Header {
    uint16_t magic; // Magic bytes for identification.
    uint8_t font_mode; // PSF font mode.
    uint8_t character_height; // PSF character size.
} ;

//make it readable without comments
extern char _binary_font_psf_start;
extern char _binary_font_psf_size_;


struct framebuffer_t {
  uint32_t *address;
  uint32_t bytes_per_fb_row;
  uint32_t width;
  uint32_t height;
  uint8_t bpp;
};

struct framebuffer_t FRAMEBUFFER;

//see gnu multiboot2 specification
bool read_boot_info_multiboot2(unsigned long multiboot2_magic, unsigned long multiboot2_info_addr)
{  
  struct multiboot_tag *tag; //in mulitboot.h, by specification
  unsigned size; //shorthand for unsigned int

  if (multiboot2_magic != MULTIBOOT2_BOOTLOADER_MAGIC)
    {
      return false;
    }

  if (multiboot2_info_addr & 7)
    {
      return false;
    }

	//the first thing at the info addr is just a uint32 total_size by the specification - header. so we dereference it to 
	//get the value
  size = *(unsigned *) multiboot2_info_addr;

  for (tag = (struct multiboot_tag *) (multiboot2_info_addr + 8); //init loop,runs once (like i=0): skip over total_size and reserved header - adding 8 to addr value so 8 down
       tag->type != MULTIBOOT_TAG_TYPE_END; //this is the break condition of the for loop
			 //here we want to move the pointer by the right number of bytes. tag->size tells u how big it is, 
			 //but adding n to a uint32 will move it by n*4 bytes. so we cast tag a byte first the bytes, then cast back
			 //& to ensure 8 byte alignment
       tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag 
                                       + ((tag->size + 7) & ~7)))
    {
      // printf ("Tag 0x%x, Size 0x%x\n", tag->type, tag->size);
      switch (tag->type)
        {
        case MULTIBOOT_TAG_TYPE_MMAP:
          {
            /*multiboot_memory_map_t *mmap;*/

            // printf ("mmap\n");
            /**/
            /*for (mmap = ((
            *     (multiboot_uint8_t *) mmap */
            /*       < (multiboot_uint8_t *) tag + tag->size;*/
            /*     mmap = (multiboot_memory_map_t *) */
            /*       ((unsigned long) mmap*/
            /*        + ((struct multiboot_tag_mmap *) tag)->entry_size))*/
              // printf (" base_addr = 0x%x%x,"
                      // " length = 0x%x%x, type = 0x%x\n",
                      // (unsigned) (mmap->addr >> 32),
                      // (unsigned) (mmap->addr & 0xffffffff),
                      // (unsigned) (mmap->len >> 32),
                      // (unsigned) (mmap->len & 0xffffffff),
                      // (unsigned) mmap->type);
          }
          break;
        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
          {
            multiboot_uint32_t color;
			unsigned i;
			struct multiboot_tag_framebuffer *tagfb
				= (struct multiboot_tag_framebuffer *) tag;

			//so this is really interesting, this is a compound literal - it creates an unnamed variable on the stack - %(rpb)4 etc -  and copies it into FRAMEBUFFEr
			//HOWEVER, the compiler recognises this and optimising by just copying the values directly rather than allocate ->  copy. Good example of compiler optimisations
			//Removes redunandt instructions
			FRAMEBUFFER = (struct framebuffer_t)
			{
				.address = (uint32_t *)(uintptr_t)  tagfb->common.framebuffer_addr,
				.bytes_per_fb_row = tagfb->common.framebuffer_pitch,
				.width   = tagfb->common.framebuffer_width,
				.height  = tagfb->common.framebuffer_height,
				.bpp     = tagfb->common.framebuffer_bpp
			};

            void *fb = (void *) (unsigned long) tagfb->common.framebuffer_addr;



            switch (tagfb->common.framebuffer_type)
              {
								//in 32bit we will be in rgb mode - so no need to worry about this
              case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED:
                {
                  unsigned best_distance, distance;
                  struct multiboot_color *palette;
            
                  palette = tagfb->framebuffer_palette;

                  color = 0;
                  best_distance = 4*256*256;
            
                  for (i = 0; i < tagfb->framebuffer_palette_num_colors; i++)
                    {
                      distance = (0xff - palette[i].blue) 
                        * (0xff - palette[i].blue)
                        + palette[i].red * palette[i].red
                        + palette[i].green * palette[i].green;
                      if (distance < best_distance)
                        {
                          color = i;
                          best_distance = distance;
                        }
                    }
                }
                break;

              case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
                color = ((1 << tagfb->framebuffer_blue_mask_size) - 1) 
                  << tagfb->framebuffer_blue_field_position;
                break;

              case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
                color = '\\' | 0x0100;
                break;

              default:
                color = 0xffffffff;
                break;
              }

            break;
          }

        }
    }
  tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag 
                                  + ((tag->size + 7) & ~7));

  // printf ("Total mbi size 0x%x\n", (unsigned) tag - addr);
	return true;
}    

//psf1 has a width of 8 bits and character size bits height. so its character size width 
char* font_glyphs;
uint8_t character_height;
uint8_t character_width = 8;


bool parse_font()
{
	struct PSF1_Header *header = (struct PSF1_Header*)&_binary_font_psf_start;
	if(header->magic != 0x0436)
		return false;

	character_height = header->character_height;
	font_glyphs = (char*)header + sizeof(struct PSF1_Header);

	return true;
}

int cx = 0;
int cy = 0;

uint32_t fg = 0x00FFFFFF;
uint32_t bg = 0x00000000;

//4 bytes per pixel. 32 bbp. - i can represent more colors
void put_pixel(int pos_x, int pos_y, uint32_t color)
{
    PIXEL* location = (PIXEL*)((char*)FRAMEBUFFER.address + FRAMEBUFFER.bytes_per_fb_row * pos_y + pos_x * 4);
    *location = color;
}


//TODO: SIMD in asm - compiler sucks at autovectorisation
void* memcpy(void* dest, const void* src, size_t count)
{
	size_t i = 0; 

	//cpu always read at its word size - size_t is always at word size of architecture.
	size_t n_words = count / sizeof(size_t);
	size_t remaining_bytes = count % sizeof(size_t);

	size_t* dest_ptr_word = (size_t*)dest;
	const size_t* src_ptr_word = (const size_t*)src;

	while(i < n_words)
	{
		dest_ptr_word[i] = src_ptr_word[i];
		i++;
	}

	char* dest_ptr_byte = (char*)&dest_ptr_word[i];
	const char* src_ptr_byte = (const char*)&src_ptr_word[i];

	i = 0;
	while(i < remaining_bytes)
	{
		dest_ptr_byte[i] = src_ptr_byte[i];	
		i++;
	}
}

void scroll_terminal()
{
	void* dest = (void*)FRAMEBUFFER.address;
	//1 row from here 
	void* src = (void*)((char*)FRAMEBUFFER.address + (FRAMEBUFFER.bytes_per_fb_row * character_height));

	size_t count = FRAMEBUFFER.bytes_per_fb_row * (FRAMEBUFFER.height - character_height);
	memcpy(dest, src, count);

	//clear final row
	uint32_t i = 0;
	PIXEL* final_character_row_start = (PIXEL*)((char*)FRAMEBUFFER.address + 
			(FRAMEBUFFER.height - character_height) * FRAMEBUFFER.bytes_per_fb_row); 

	while(i < (FRAMEBUFFER.bytes_per_fb_row / sizeof(PIXEL)) * character_height)
	{
		final_character_row_start[i] = bg;
		i++;
	}
}

void increment_character_row()
{
	cx = 0;
	cy += character_height;
	//cant reliably write another character height wise
	if(cy >= FRAMEBUFFER.height - character_height)
	{
		scroll_terminal();
		cy -= character_height;
		return;
	}
}


//move framebuffer up. so from row worth of bytes from the top. copy to where we're at. then copy them to the top.
void draw_char(char input)
{
	//lets get the start byte for that glyph. char = 0 -> glyph one and so one
	char* starting_glyph_byte = (font_glyphs + input*character_height);

	for(size_t y = 0; y < character_height; y++)
	{
		for(size_t x = 0; x < character_width; x ++)
		{
			if(*(starting_glyph_byte + y) & (0x80 >> x))
				put_pixel(cx + x, cy + y, fg);
		}
	}
}

void draw_string(char* input)
{
	while(*input)
	{
		if(*input == '\n')
		{
			increment_character_row();
			input++;
			continue;
		}

		//from where we're drawing we cant reliably (or at all) fit another character
		draw_char(*input);
		input++;
		cx += character_width;
		if(cx >= FRAMEBUFFER.width - character_width)
		{
			increment_character_row();
		}
	}
}

void kernel_main(unsigned long multiboot2_magic, unsigned long multiboot2_info_addr) 
{
	bool boot_success = read_boot_info_multiboot2(multiboot2_magic, multiboot2_info_addr);
	bool font_success = parse_font();

	if(!boot_success || !font_success)
		return;

	cy = FRAMEBUFFER.height - character_height;


	//GOAL: have these two on top of eachother. 
	
	//after first draw_string we should have Hello WOrlds and an empty row under it
	draw_string("Hello, world\n");
	draw_string("Hello, world\n");
	draw_string("Hello, world");
}
