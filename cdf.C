Double_t cdf(Double_t *x, Double_t *p)
{
	return p[0]*(ROOT::Math::normal_cdf (*x, p[2], p[1]));
}