#if LITECORE_PACKAGED
    internal
#else
    public
#endif 
    unsafe struct FLDictKey
    {
        #pragma warning disable CS0169

        // _private1[3] 
        private void* _private1a;
        private void* _private1b;
        private void* _private1c;
        private uint _private2;
        private byte _private3;

        #pragma warning restore CS0169
    }
