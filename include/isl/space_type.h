#ifndef ISL_SPACE_TYPE_H
#define ISL_SPACE_TYPE_H

#include <isl/ctx.h>
#include <isl/list.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct __isl_export isl_space;
typedef struct isl_space isl_space;

ISL_DECLARE_LIST_TYPE(space)

enum isl_dim_type {
	isl_dim_cst,
	isl_dim_param,
	isl_dim_in,
	isl_dim_out,
	isl_dim_set = isl_dim_out,
	isl_dim_div,
	isl_dim_all
};

#if defined(__cplusplus)
}
#endif

#endif
