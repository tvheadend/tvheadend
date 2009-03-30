struct filebundle_entry {
  const char *filename;
  const unsigned char *data;
  int size;
  int original_size; // -1 if file is not compressed
};

struct filebundle {
  struct filebundle *next;
  const struct filebundle_entry *entries;
  const char *prefix;
};
