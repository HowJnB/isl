#include "isl_sample.h"
#include "isl_sample_piplib.h"
#include "isl_vec.h"
#include "isl_mat.h"
#include "isl_map_private.h"
#include "isl_equalities.h"

static struct isl_vec *point_sample(struct isl_ctx *ctx,
	struct isl_basic_set *bset)
{
	struct isl_vec *sample;
	isl_basic_set_free(ctx, bset);
	sample = isl_vec_alloc(ctx, 1);
	if (!sample)
		return NULL;
	isl_int_set_si(sample->block.data[0], 1);
	return sample;
}

static struct isl_vec *interval_sample(struct isl_ctx *ctx,
	struct isl_basic_set *bset)
{
	struct isl_vec *sample;

	bset = isl_basic_set_simplify(ctx, bset);
	if (!bset)
		return NULL;
	if (bset->n_eq > 0)
		return isl_basic_set_sample(ctx, bset);
	sample = isl_vec_alloc(ctx, 2);
	isl_int_set_si(sample->block.data[0], 1);
	if (bset->n_ineq == 0)
		isl_int_set_si(sample->block.data[1], 0);
	else {
		int i;
		isl_int t;
		isl_int_init(t);
		if (isl_int_is_one(bset->ineq[0][1]))
			isl_int_neg(sample->block.data[1], bset->ineq[0][0]);
		else
			isl_int_set(sample->block.data[1], bset->ineq[0][0]);
		for (i = 1; i < bset->n_ineq; ++i) {
			isl_seq_inner_product(sample->block.data,
						bset->ineq[i], 2, &t);
			if (isl_int_is_neg(t))
				break;
		}
		isl_int_clear(t);
		if (i < bset->n_ineq) {
			isl_vec_free(ctx, sample);
			sample = isl_vec_alloc(ctx, 0);
		}
	}
	isl_basic_set_free(ctx, bset);
	return sample;
}

static struct isl_mat *independent_bounds(struct isl_ctx *ctx,
	struct isl_basic_set *bset)
{
	int i, j, n;
	struct isl_mat *dirs = NULL;

	if (!bset)
		return NULL;

	if (bset->n_ineq == 0)
		return isl_mat_alloc(ctx, 0, bset->dim);

	dirs = isl_mat_alloc(ctx, bset->dim, bset->dim);
	if (!dirs)
		return NULL;
	isl_seq_cpy(dirs->row[0], bset->ineq[0]+1, dirs->n_col);
	for (j = 1, n = 1; n < bset->dim && j < bset->n_ineq; ++j) {
		int pos;

		isl_seq_cpy(dirs->row[n], bset->ineq[j]+1, dirs->n_col);

		pos = isl_seq_first_non_zero(dirs->row[n], dirs->n_col);
		if (pos < 0)
			continue;
		for (i = 0; i < n; ++i) {
			int pos_i;
			pos_i = isl_seq_first_non_zero(dirs->row[i], dirs->n_col);
			if (pos_i < pos)
				continue;
			if (pos_i > pos)
				break;
			isl_seq_elim(dirs->row[n], dirs->row[i], pos,
					dirs->n_col, NULL);
			pos = isl_seq_first_non_zero(dirs->row[n], dirs->n_col);
			if (pos < 0)
				break;
		}
		if (pos < 0)
			continue;
		if (i < n) {
			int k;
			isl_int *t = dirs->row[n];
			for (k = n; k > i; --k)
				dirs->row[k] = dirs->row[k-1];
			dirs->row[i] = t;
		}
		++n;
	}
	dirs->n_row = n;
	return dirs;
}

static struct isl_basic_set *remove_lineality(struct isl_ctx *ctx,
	struct isl_basic_set *bset, struct isl_mat *bounds, struct isl_mat **T)
{
	struct isl_mat *U = NULL;
	unsigned old_dim, new_dim;

	old_dim = bset->dim;
	new_dim = bounds->n_row;
	*T = NULL;
	bounds = isl_mat_left_hermite(ctx, bounds, 0, &U, NULL);
	if (!bounds)
		goto error;
	U = isl_mat_lin_to_aff(ctx, U);
	U = isl_mat_drop_cols(ctx, U, 1 + new_dim, old_dim - new_dim);
	bset = isl_basic_set_preimage(ctx, bset, isl_mat_copy(ctx, U));
	if (!bset)
		goto error;
	*T = U;
	isl_mat_free(ctx, bounds);
	return bset;
error:
	isl_mat_free(ctx, bounds);
	isl_mat_free(ctx, U);
	isl_basic_set_free(ctx, bset);
	return NULL;
}

struct isl_vec *isl_basic_set_sample(struct isl_ctx *ctx,
	struct isl_basic_set *bset)
{
	struct isl_mat *bounds;
	if (!bset)
		return NULL;

	if (F_ISSET(bset, ISL_BASIC_SET_EMPTY)) {
		isl_basic_set_free(ctx, bset);
		return isl_vec_alloc(ctx, 0);
	}

	isl_assert(ctx, bset->nparam == 0, goto error);
	isl_assert(ctx, bset->n_div == 0, goto error);

	if (bset->n_eq > 0) {
		struct isl_mat *T;
		struct isl_vec *sample;

		bset = isl_basic_set_remove_equalities(ctx, bset, &T, NULL);
		sample = isl_basic_set_sample(ctx, bset);
		if (sample && sample->size != 0)
			sample = isl_mat_vec_product(ctx, T, sample);
		else
			isl_mat_free(ctx, T);
		return sample;
	}
	if (bset->dim == 0)
		return point_sample(ctx, bset);
	if (bset->dim == 1)
		return interval_sample(ctx, bset);
	bounds = independent_bounds(ctx, bset);
	if (!bounds)
		goto error;
	if (bounds->n_row == bset->dim)
		isl_mat_free(ctx, bounds);
	else {
		struct isl_mat *T;
		struct isl_vec *sample;

		bset = remove_lineality(ctx, bset, bounds, &T);
		sample = isl_basic_set_sample(ctx, bset);
		if (sample && sample->size != 0)
			sample = isl_mat_vec_product(ctx, T, sample);
		else
			isl_mat_free(ctx, T);
		return sample;
	}
	return isl_pip_basic_set_sample(ctx, bset);
error:
	isl_basic_set_free(ctx, bset);
	return NULL;
}
