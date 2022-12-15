#!/bin/bash

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
spack mirror add --scope site \
  --s3-access-key-id "${SBCACHE_AWS_ID:-$SBCACHE_AWS_ID_NP}" \
  --s3-access-key-secret "${SBCACHE_AWS_SECRET:-$SBCACHE_AWS_SECRET_NP}" \
  destination "$SBCACHE_URL/$root/$subdir" \
  || exit $?

# For merge request pipelines targeting a protected branch, also add the target's buildcache
if test "$CI_MERGE_REQUEST_TARGET_BRANCH_PROTECTED" = "true"; then
  spack mirror add --scope site \
    --s3-access-key-id "${SBCACHE_AWS_ID:-$SBCACHE_AWS_ID_NP}" \
    --s3-access-key-secret "${SBCACHE_AWS_SECRET:-$SBCACHE_AWS_SECRET_NP}" \
    main-target "$SBCACHE_URL/protected/$CI_MERGE_REQUEST_TARGET_BRANCH_NAME" \
    || exit $?
fi

# Always try to reuse from the default branch as well, if we didn't add it in the previous step
if test "$CI_MERGE_REQUEST_TARGET_BRANCH_NAME" != "$CI_DEFAULT_BRANCH" \
   && test "$CI_COMMIT_REF_NAME" != "$CI_DEFAULT_BRANCH"; then
  spack mirror add --scope site \
    --s3-access-key-id "${SBCACHE_AWS_ID:-$SBCACHE_AWS_ID_NP}" \
    --s3-access-key-secret "${SBCACHE_AWS_SECRET:-$SBCACHE_AWS_SECRET_NP}" \
    main-default "$SBCACHE_URL/protected/$CI_DEFAULT_BRANCH" \
    || exit $?
fi

spack mirror list --scope site
