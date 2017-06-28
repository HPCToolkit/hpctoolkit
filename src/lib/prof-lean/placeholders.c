void *
canonicalize_placeholder(void *routine)
{
#if (HOST_LITTLE_ENDIAN == 0) & HOST_CPU_PPC
    // with the PPC 64-bit ABI on big-endian systems, functions are represented by D symbols and require one level of indirection
    void *routine_addr = *(void**) routine;
#else
    void *routine_addr = (void *) routine;
#endif
    return routine_addr;
}

