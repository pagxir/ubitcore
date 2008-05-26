struct segment_t {

    segment_t * seg_next;

    const void *seg_lastid;

    int seg_time;

    int seg_piece;

    int seg_offset;

    int seg_length;
};
int bitfield_set(char *bitfield, int piece);
int bitfield_clr(char *bitfield, int piece);
int segbuf_write(int piece, int offset,                 const char buffer[], int len,                 const char *bitfield);
int assign_segment(segment_t * segment, const char *bitfield);
int bitfield_test(const char bitField[], int index);
int reassign_segment(const char *id);
int segbuf_read(int piece, int start, char buffer[], int len);

