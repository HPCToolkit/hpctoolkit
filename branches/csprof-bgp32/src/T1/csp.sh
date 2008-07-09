csp()
{
    CSPROF_OPT_EVENT=WALLCLOCK:5000 "$@"
}
dcsp()
{
    CSPROF_OPT_EVENT=WALLCLOCK:5000 CSPROF_DD='UNW_INIT UNWIND SAMPLE_FILTER' "$@"
}
lc()
{
    ls -l *.csp
}
ll()
{
    ls -l *.csl
}
rc()
{
    rm -rf *.csp *.csl experiment*
}
