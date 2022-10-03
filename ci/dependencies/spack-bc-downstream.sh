#!/bin/bash

# Copy the AWS authentication flags into the Spack mirror config. Implements the principle of
# least privilege.
if test "$CI_COMMIT_REF_PROTECTED" = "true";
then SBCACHE_ROOT=protected;
else SBCACHE_ROOT=nonprotected;
fi
if test -n "$CI_MERGE_REQUEST_IID";
then SBCACHE_DIR="mr${CI_MERGE_REQUEST_IID}_${CI_COMMIT_REF_SLUG}";
else SBCACHE_DIR="$CI_COMMIT_REF_SLUG";
fi
spack mirror add --scope site \
  --s3-access-key-id "${SBCACHE_AWS_ID:-$SBCACHE_AWS_ID_NP}" \
  --s3-access-key-secret "${SBCACHE_AWS_SECRET:-$SBCACHE_AWS_SECRET_NP}" \
  destination "$SBCACHE_SCHEMA://$SBCACHE_BUCKET/$SBCACHE_ROOT/$SBCACHE_DIR" \
  || exit $?

# For merge request pipelines targeting a protected branch, also add the target's buildcache
if test "$CI_MERGE_REQUEST_TARGET_BRANCH_PROTECTED" = "true"; then
  spack mirror add --scope site \
    --s3-access-key-id "${SBCACHE_AWS_ID:-$SBCACHE_AWS_ID_NP}" \
    --s3-access-key-secret "${SBCACHE_AWS_SECRET:-$SBCACHE_AWS_SECRET_NP}" \
    main-target "$SBCACHE_SCHEMA://$SBCACHE_BUCKET/protected/$CI_MERGE_REQUEST_TARGET_BRANCH_NAME" \
    || exit $?
fi

spack mirror list
