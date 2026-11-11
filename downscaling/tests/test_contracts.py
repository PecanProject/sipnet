import unittest
import numpy as np
import pandas as pd
from downscaling.contracts import *

class ContractTests(unittest.TestCase):
    def test_rootzone_mass_to_volume(self):
        np.testing.assert_allclose(rootzone([[20,60,120,900]]),[.2])
        self.assertTrue(np.isnan(rootzone([[20,np.nan,120,900]])[0]))
    def test_ppfd_physical_conversion(self):
        self.assertAlmostEqual(float(ppfd(10800*100)),205.65)
    def test_short_gap_dates_and_long_gap(self):
        dates=pd.to_datetime(['2020-12-18','2021-01-01','2021-01-17','2021-02-02'],utc=True)
        x,flag=short_gaps([0,np.nan,np.nan,1],dates,['Aqua']*4)
        np.testing.assert_allclose(x,[0,14/46,30/46,1]);self.assertEqual(flag.tolist(),[0,1,1,0])
        x,_=short_gaps([0,np.nan,np.nan,np.nan,1],pd.date_range('2020-01-01',periods=5),['Aqua']*5)
        self.assertTrue(np.isnan(x[1:4]).all())
    def test_no_extrapolation_or_cross_sensor_fill(self):
        x,f=short_gaps([np.nan,0,np.nan,1,np.nan],pd.date_range('2020-01-01',periods=5),['Aqua','Aqua','Aqua','Terra','Terra'])
        self.assertTrue(np.isnan(x[[0,2,4]]).all());self.assertFalse(f.any())
    def test_duplicate_and_absent_keys_fail(self):
        for f in [pd.DataFrame({'index':[1,1],'utc':[1,1]}),pd.DataFrame({'index':[1,None],'utc':[1,2]})]:
            with self.assertRaises(ValueError):unique(f,['index','utc'])
    def test_exact_window_precipitation_and_support(self):
        dates=pd.date_range('2020-02-29',periods=9,freq='3h',tz='UTC')
        f=pd.DataFrame(dict(index=1,utc=dates,t2m=273.15,d2m=273.15,tp=.001,ssrd=1080000,u10=3.,v10=4.,EVI=.2,NDVI=.3,**{n:100. for n in SOIL}))
        x=aggregate_period(f,dates[0],dates[-1]).iloc[0]
        self.assertAlmostEqual(x.precipitation_m,.008);self.assertEqual(x.expected_steps,8)
        self.assertAlmostEqual(x.gldas_rootzone_vwc,.3);self.assertAlmostEqual(x.ppfd_umol_m2_s,205.65)
        f.loc[0,'tp']=np.nan;x=aggregate_period(f,dates[0],dates[-1]).iloc[0]
        self.assertTrue(np.isnan(x.precipitation_m));self.assertEqual(x.precipitation_m_support,7)
        with self.assertRaises(ValueError):aggregate_period(f.iloc[1:],dates[0],dates[-1])

if __name__=='__main__':unittest.main()


def test_calendar_lags_and_clipped_support(tmp_path):
    import pytest
    from downscaling.calendar import read_periods,selection
    frame=pd.DataFrame({'period_id':list(range(-3,2)),'period_key':pd.date_range('2019-12-27',periods=5).astype(str),
                        'start':pd.date_range('2019-12-27',periods=5),'stop':pd.date_range('2019-12-28',periods=5)})
    path=tmp_path/'periods.csv';frame.to_csv(path,index=False)
    calendar=read_periods(path);needed,wanted=selection(calendar,[1])
    assert len(needed)==5 and len(wanted)==1
    with pytest.raises(ValueError,match='lag support'):selection(calendar,[2])


def test_feature_order_and_missingness():
    frame=pd.DataFrame({n:[.2,.3] for n in STATIC+DYNAMIC+[f'{n}_L{k}' for k in range(1,5) for n in DYNAMIC]})
    frame['lat']=[45,60];frame['lon']=[-90,-100];frame['period_key']=['2020-02-29','2020-07-19'];frame['LC_proxy']=[1,11]
    values,lc,valid=predictors(frame)
    assert values.shape==(2,67) and values.dtype==np.float32 and valid.all()
    assert lc.tolist()==[1,11]
    frame.loc[0,'temp_c']=np.nan
    assert predictors(frame)[2].tolist()==[False,True]
