def flatten(lists):
    """Given a possibly-recursive list, returned the flattened form"""
    result = []
    for x in lists:
        if isinstance(x, list):
            result.extend(flatten(x))
        else:
            result.append(x)
    return result
