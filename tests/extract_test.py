from concurrent.futures import ThreadPoolExecutor
from threading import Barrier
import pytest
from common import *

def test_extract(ds_local, ds_trimmed):
    ds = ds_local
    ds_extracted = ds.extract()
    ds_extracted.x.tolist() == ds_trimmed.x.tolist()
    ds_extracted.x.tolist() == np.arange(10.).tolist()
    assert len(ds_extracted) == len(ds_trimmed) == 10
    assert ds_extracted.length_original() == ds_trimmed.length_original() == 10
    assert ds_extracted.length_unfiltered() == ds_trimmed.length_unfiltered() == 10
    assert ds_extracted.filtered is False

    ds_extracted2 = ds_extracted[ds_extracted.x >= 5].extract()
    ds_extracted2.x.tolist() == np.arange(5,10.).tolist()
    assert len(ds_extracted2) == 5
    assert ds_extracted2.length_original() == 5
    assert ds_extracted2.length_unfiltered() == 5
    assert ds_extracted2.filtered is False



def test_thread_safe():
    df = vaex.from_arrays(x=np.arange(int(1e5)))
    dff = df[df.x < 100]

    barrier = Barrier(100)
    def run(_ignore):
        barrier.wait()
        # now we all do the extract at the same time
        dff.extract()
    pool = ThreadPoolExecutor(max_workers=100)
    _values = list(pool.map(run, range(100)))


def test_extract_empty():
    df = vaex.from_arrays(x=np.arange(10))
    df = df[df.x < 0]
    df_extracted = df.extract()
    assert len(df_extracted) == 0
    assert df_extracted.length_original() == 0
    assert df_extracted.length_unfiltered() == 0
    assert df_extracted.filtered is False

    df['z'] = df.x + 1
    with pytest.raises(ValueError, match='Cannot extract a DataFrame with.*'):
        df_extracted = df.extract()
