#!/bin/sh

# Generate a Spack mirrors.yaml file containing a series of mirrors for our use
echo 'mirrors:'

# Copy the AWS authentication flags into the Spack mirror config. Implements the principle of
# least privilege.
if test "$CI_COMMIT_REF_PROTECTED" = "true";
then root=protected;
else root=nonprotected;
fi
if test -n "$CI_MERGE_REQUEST_IID";
then subdir="mr${CI_MERGE_REQUEST_IID}_${CI_COMMIT_REF_SLUG}";
else subdir="$CI_COMMIT_REF_SLUG";
fi
echo '  destination:'
echo '    fetch:'
echo "      url: $SBCACHE_URL/$root/$subdir"
echo "      access_pair:"
echo "      - ${SBCACHE_AWS_ID:-$SBCACHE_AWS_ID_NP}"
echo "      - ${SBCACHE_AWS_SECRET:-$SBCACHE_AWS_SECRET_NP}"
echo '    push:'
echo "      url: $SBCACHE_URL/$root/$subdir"
echo "      access_pair:"
echo "      - ${SBCACHE_AWS_ID:-$SBCACHE_AWS_ID_NP}"
echo "      - ${SBCACHE_AWS_SECRET:-$SBCACHE_AWS_SECRET_NP}"

# For merge request pipelines targeting a protected branch, also add the target's buildcache
if test "$CI_MERGE_REQUEST_TARGET_BRANCH_PROTECTED" = "true"; then
echo '  main-target:'
echo '    fetch:'
echo "      url: $SBCACHE_URL/protected/$CI_MERGE_REQUEST_TARGET_BRANCH_NAME"
echo "      access_pair:"
echo "      - ${SBCACHE_AWS_ID:-$SBCACHE_AWS_ID_NP}"
echo "      - ${SBCACHE_AWS_SECRET:-$SBCACHE_AWS_SECRET_NP}"
echo '    push:'
echo "      url: $SBCACHE_URL/protected/$CI_MERGE_REQUEST_TARGET_BRANCH_NAME"
echo "      access_pair:"
echo "      - ${SBCACHE_AWS_ID:-$SBCACHE_AWS_ID_NP}"
echo "      - ${SBCACHE_AWS_SECRET:-$SBCACHE_AWS_SECRET_NP}"
fi

# Always try to reuse from the default branch as well, if we didn't add it in the previous step
if test "$CI_MERGE_REQUEST_TARGET_BRANCH_NAME" != "$CI_DEFAULT_BRANCH" \
   && test "$CI_COMMIT_REF_NAME" != "$CI_DEFAULT_BRANCH"; then
echo '  main-default:'
echo '    fetch:'
echo "      url: $SBCACHE_URL/protected/$CI_DEFAULT_BRANCH"
echo "      access_pair:"
echo "      - ${SBCACHE_AWS_ID:-$SBCACHE_AWS_ID_NP}"
echo "      - ${SBCACHE_AWS_SECRET:-$SBCACHE_AWS_SECRET_NP}"
echo '    push:'
echo "      url: $SBCACHE_URL/protected/$CI_DEFAULT_BRANCH"
echo "      access_pair:"
echo "      - ${SBCACHE_AWS_ID:-$SBCACHE_AWS_ID_NP}"
echo "      - ${SBCACHE_AWS_SECRET:-$SBCACHE_AWS_SECRET_NP}"
fi
