void *
canonicalize_placeholder(void *routine)
{
#ifdef __PPC64__
    // with the PPC 64-bit ABI, functions are represented by D symbols and require one level of indirection
    void *routine_addr = *(void**) routine;
#else
    void *routine_addr = (void *) routine;
#endif
    return routine_addr;
}

