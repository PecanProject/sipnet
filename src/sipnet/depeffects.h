#ifndef DEP_EFFECTS_H
#define DEP_EFFECTS_H

/**
 * Calculate water fraction bounded in [0,1]
 *
 * Keeps moisture dependency terms bounded in [0, 1]. In configurations with
 * WATER_HRESP=1 and ANAEROBIC=0 (e.g., russell_1 smoke test), this prevents
 * super-saturated soil water (water > whc) from boosting respiration above
 * the full-wet response. Also used to constrain water frac for evapotrans
 * calcs.
 *
 * @param water current water level
 * @param whc water holding capacity
 * @return bounded water fraction
 */
double getClippedWaterFrac(double water, double whc);

/**
 * Calculate the anaerobic index
 *
 * Calculate the index from current soil moisture, soil WHC, and the
 * fAnoxia parameter
 *
 * @param water current soil water
 * @param whc soil water holding capacity
 * @return anaerobic index
 */
double calcAnaerobicIndex(double water, double whc);

/*!
 *  Calculate heterotrophic respiration moisture dependency effect
 *
 *  This dependency term is used in soil respiration and litter breakdown
 *
 *  Calculation also depends on soil temperature and the options waterHResp and
 *  anaerobic.
 *
 * @param water current soil water
 * @param whc water holding capacity
 * @return moisture effect as a fraction between 0 and 1
 */
double calcRespMoistEffect(double water, double whc);

/*!
 * Calculate methane production moisture dependency effect
 *
 * Methane production increases monotonically with anoxia, so we calculate
 * this dependency term as power of the anaerobic index.
 *
 * This dependency term is used in methane production
 *
 * Calculation also depends on soil temperature.
 *
 * @param water current soil water
 * @param whc water holding capacity
 * @return moisture effect as a fraction between 0 and 1
 */
double calcMethaneMoistEffect(double water, double whc);

/**
 * Calculate temperature dependency effect
 *
 * Temperature dependency term is used in soil respiration, litter
 * breakdown, and nitrogen volatilization.
 *
 * @param tsoil soil temperature
 * @return temperature effect as a fraction between 0 and 1
 */
double calcTempEffect(double tsoil);

/**
 * Calculate effect of tillage on soil respiration or litter breakdown
 */
double calcTillageEffect(void);

/**
 * Calculate C:N ratio dependency effect
 *
 * C:N ratio  dependency term is used in soil respiration and litter
 * breakdown.
 *
 * @param kCN CN dependency control param for soil/litter
 * @param poolC Current size of carbon pool
 * @param poolN Current size of nitrogen pool
 * @return C:N ratio effect as a fraction between 0 and 1
 */
double calcCNEffect(double kCN, double poolC, double poolN);

/*!
 * Calculate volatilization moisture dependency effect
 *
 * The volatilized nitrogen flux is assumed to peak at intermediate redox
 * conditions, where aerobic and anaerobic processes overlap.
 *
 * This dependency term is used in nitrogen volatilization
 *
 * Calculation also depends on soil temperature.
 *
 * @param water current soil water
 * @param whc water holding capacity
 * @return moisture effect as a fraction between 0 and 1
 */
double calcVolatilizationMoistEffect(double water, double whc);

#endif  // DEP_EFFECTS_H
