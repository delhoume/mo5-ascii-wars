#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE 256
#define SECTORS_PER_TRACK 16
#define TRACKS 80
#define DISK_SIZE (SECTOR_SIZE * SECTORS_PER_TRACK * TRACKS)
#define BLOCK_SIZE (SECTOR_SIZE * 8)
#define LOGICAL_SECTOR_SIZE 255
#define LOGICAL_BLOCK_SIZE (LOGICAL_SECTOR_SIZE * 8)
#define BLOCK_COUNT (TRACKS * 2)
#define FAT_OFFSET (20 * SECTOR_SIZE * SECTORS_PER_TRACK + SECTOR_SIZE + 1)
#define DIRECTORY_OFFSET (20 * SECTOR_SIZE * SECTORS_PER_TRACK + 2 * SECTOR_SIZE)
#define DIRECTORY_SIZE (14 * SECTOR_SIZE)
#define DIRECTORY_ENTRY_SIZE 32
#define DIRECTORY_ENTRY_COUNT (DIRECTORY_SIZE / DIRECTORY_ENTRY_SIZE)
#define FREE_BLOCK 0xff
#define RESERVED_BLOCK 0xfe

typedef unsigned char byte;

static byte disk[DISK_SIZE];
static unsigned int references[BLOCK_COUNT];

static unsigned int fat_value(unsigned int block)
{
	return disk[FAT_OFFSET + block];
}

static void print_field(const byte *data, size_t size)
{
	size_t end = size;

	while (end > 0 && data[end - 1] == ' ')
	{
		end--;
	}
	for (size_t i = 0; i < end; i++)
	{
		putchar(data[i] >= 0x20 && data[i] <= 0x7e ? data[i] : '.');
	}
}

static void print_filename(const byte *entry)
{
	print_field(entry, 8);
	putchar('.');
	print_field(entry + 8, 3);
}

static const char *file_type(byte value)
{
	switch (value)
	{
		case 0: return "BASIC program";
		case 1: return "BASIC data";
		case 2: return "machine-code program";
		case 3: return "text";
		default: return "unknown";
	}
}

static void print_block(unsigned int block)
{
	unsigned int track = block / 2;
	unsigned int first_sector = block % 2 == 0 ? 1 : 9;

	printf("%u(T%02u:S%02u-S%02u)", block, track, first_sector,
	       first_sector + 7);
}

static void print_fat(void)
{
	unsigned int free_count = 0;
	unsigned int reserved_count = 0;
	unsigned int linked_count = 0;
	unsigned int eof_count = 0;
	unsigned int invalid_count = 0;

	printf("\nAllocation table (track 20, sector 2; 160 block entries)\n");
	for (unsigned int block = 0; block < BLOCK_COUNT; block++)
	{
		unsigned int value = fat_value(block);

		if (value == FREE_BLOCK)
			free_count++;
		else if (value == RESERVED_BLOCK)
			reserved_count++;
		else if (value < BLOCK_COUNT)
			linked_count++;
		else if (value >= 0xc1 && value <= 0xc8)
			eof_count++;
		else
			invalid_count++;

		printf("  %3u:", block);
		print_block(block);
		printf("  FAT=%02X  ", value);
		if (value == FREE_BLOCK)
			printf("free");
		else if (value == RESERVED_BLOCK)
			printf("reserved");
		else if (value < BLOCK_COUNT)
			printf("next block %u", value);
		else if (value >= 0xc1 && value <= 0xc8)
			printf("end of file, %u sector(s) used in final block",
			       value - 0xc0);
		else
			printf("invalid FAT value");
		putchar('\n');
	}
	printf("  Summary: %u free, %u reserved, %u linked, %u end-of-file, "
	       "%u invalid entries\n",
	       free_count, reserved_count, linked_count, eof_count, invalid_count);
}

static void print_entry_bytes(const byte *entry)
{
	printf("    Raw entry:");
	for (unsigned int i = 0; i < DIRECTORY_ENTRY_SIZE; i++)
	{
		if (i % 16 == 0)
			printf("\n      ");
		printf("%02X ", entry[i]);
	}
	putchar('\n');
}

static void print_file_chain(const byte *entry, unsigned int entry_number)
{
	unsigned int block = entry[13];
	unsigned int used_last_sector = ((unsigned int)entry[14] << 8) | entry[15];
	unsigned char visited[BLOCK_COUNT] = {0};
	unsigned int chain_length = 0;
	unsigned int sectors_used = 0;
	unsigned int final_block_sectors = 0;
	unsigned long long file_size = 0;
	int complete = 0;

	printf("    Chain: ");
	while (block < BLOCK_COUNT)
	{
		unsigned int next;

		if (visited[block])
		{
			printf(" -> LOOP at block %u", block);
			break;
		}
		visited[block] = 1;
		references[block]++;
		chain_length++;
		if (chain_length > 1)
			printf(" -> ");
		print_block(block);

		next = fat_value(block);
		if (next >= 0xc1 && next <= 0xc8)
		{
			unsigned int sectors = next - 0xc0;

			final_block_sectors = sectors;
			sectors_used += sectors;
			file_size += (unsigned long long)(sectors - 1) * LOGICAL_SECTOR_SIZE;
			file_size += used_last_sector;
			if (used_last_sector > LOGICAL_SECTOR_SIZE)
				printf(" [invalid final-sector byte count: %u]",
				       used_last_sector);
			complete = 1;
			break;
		}
		if (next >= BLOCK_COUNT)
		{
			printf(" -> [invalid FAT value %02X]", next);
			break;
		}
		sectors_used += 8;
		file_size += LOGICAL_BLOCK_SIZE;
		block = next;
	}
	if (entry[13] >= BLOCK_COUNT)
	{
		printf("[invalid starting block %u]", entry[13]);
	}
	if (chain_length == 0 && entry[13] < BLOCK_COUNT)
	{
		printf("[empty chain]");
	}

	printf("\n    Chain blocks: %u; ", chain_length);
	if (complete)
	{
		printf("sectors used: %u; final block uses %u of 8 sectors; "
		       "reported size: %llu bytes\n",
		       sectors_used, final_block_sectors, file_size);
	}
	else
		printf("size unavailable (incomplete or corrupt chain)\n");
	if (!complete)
		fprintf(stderr, "warning: directory entry %u has an incomplete chain\n",
		        entry_number);
}

static void print_directory(void)
{
	unsigned int active_count = 0;
	unsigned int empty_count = 0;
	unsigned int deleted_count = 0;
	unsigned int other_count = 0;

	printf("\nDirectory (track 20, sectors 3-16; %u slots)\n",
	       DIRECTORY_ENTRY_COUNT);
	for (unsigned int slot = 0; slot < DIRECTORY_ENTRY_COUNT; slot++)
	{
		const byte *entry = &disk[DIRECTORY_OFFSET + slot * DIRECTORY_ENTRY_SIZE];
		byte status = entry[0];

		if (status == FREE_BLOCK)
		{
			empty_count++;
			continue;
		}
		if (status == 0)
		{
			deleted_count++;
			continue;
		}
		if (status <= 0x20 || status >= RESERVED_BLOCK)
		{
			other_count++;
			printf("  Slot %3u: unrecognized status byte %02X\n", slot, status);
			print_entry_bytes(entry);
			continue;
		}

		active_count++;
		printf("  Slot %3u: ", slot);
		print_filename(entry);
		printf("\n    Status: %02X; file type: %u (%s); data type: %02X\n",
		       status, entry[11], file_type(entry[11]), entry[12]);
		printf("    First block: %u; final-sector byte count: %u\n",
		       entry[13], ((unsigned int)entry[14] << 8) | entry[15]);
		printf("    Date bytes (day/month/year since 2000): %u/%u/%u\n",
		       entry[24], entry[25], 2000U + entry[26]);
		printf("    Comment: ");
		if (entry[16] == 0)
			printf("(none)");
		else
			print_field(entry + 16, 8);
		putchar('\n');
		print_file_chain(entry, slot);
		print_entry_bytes(entry);
	}
	printf("  Summary: %u active, %u unused, %u deleted, %u unrecognized slots\n",
	       active_count, empty_count, deleted_count, other_count);
}

static void print_unreferenced_allocations(void)
{
	unsigned int count = 0;

	printf("\nAllocated blocks not reached from an active directory entry:\n");
	for (unsigned int block = 0; block < BLOCK_COUNT; block++)
	{
		unsigned int value = fat_value(block);

		if (references[block] == 0 && value != FREE_BLOCK &&
		    value != RESERVED_BLOCK)
		{
			printf("  ");
			print_block(block);
			printf("  FAT=%02X\n", value);
			count++;
		}
		if (references[block] > 1)
		{
			fprintf(stderr, "warning: block %u is referenced by multiple file chains\n",
			        block);
		}
	}
	if (count == 0)
		printf("  (none)\n");
}

static int load_disk(const char *path)
{
	FILE *file = fopen(path, "rb");
	size_t read_count;
	int extra;

	if (file == NULL)
	{
		perror(path);
		return 0;
	}
	read_count = fread(disk, 1, sizeof(disk), file);
	extra = fgetc(file);
	if (ferror(file))
	{
		fprintf(stderr, "error reading %s\n", path);
		fclose(file);
		return 0;
	}
	if (fclose(file) != 0)
	{
		perror(path);
		return 0;
	}
	if (read_count != sizeof(disk) || extra != EOF)
	{
		fprintf(stderr, "%s: expected an %u-byte .fd image, read %zu bytes%s\n",
		        path, (unsigned int)sizeof(disk), read_count,
		        extra != EOF ? " (file is larger than expected)" : "");
		return 0;
	}
	return 1;
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s disk.fd\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (!load_disk(argv[1]))
		return EXIT_FAILURE;

	printf("Thomson .fd disk image: %s\n", argv[1]);
	printf("Image size: %u bytes; geometry: %u tracks x %u sectors x %u bytes\n",
	       (unsigned int)sizeof(disk), TRACKS, SECTORS_PER_TRACK, SECTOR_SIZE);
	printf("Allocation blocks: %u bytes each; %u blocks total\n",
	       BLOCK_SIZE, BLOCK_COUNT);
	printf("File data uses %u bytes per sector (%u physical bytes per sector)\n",
	       LOGICAL_SECTOR_SIZE, SECTOR_SIZE);
	printf("Volume name: ");
	print_field(disk + 20 * SECTOR_SIZE * SECTORS_PER_TRACK, 8);
	putchar('\n');
	printf("Boot sector (track 0, sector 1), first 32 bytes:");
	for (unsigned int i = 0; i < 32; i++)
	{
		if (i % 16 == 0)
			printf("\n  ");
		printf("%02X ", disk[i]);
	}
	putchar('\n');
	printf("System areas: boot sector T00/S01; FAT T20/S02; "
	       "directory T20/S03-S16; blocks 40-41 reserved by fdfs\n");

	print_fat();
	print_directory();
	print_unreferenced_allocations();
	return EXIT_SUCCESS;
}
