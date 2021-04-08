"""
 * GTDynamics Copyright 2021, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 *
 * @file  test_print.py
 * @brief Test printing with DynamicsSymbol.
 * @author Gerry Chen
"""

from io import StringIO
import unittest
from unittest.mock import patch

import gtdynamics as gtd
import gtsam

class Print(unittest.TestCase):
    def test_values(self):
        """Checks that printing Values uses the GTDKeyFormatter instead of gtsam's default"""
        v = gtsam.Values()
        gtd.InsertJointAngleDouble(v, 0, 1, 2)
        self.assertIn('q(0)1', v.__repr__("values: ", gtd.GTDKeyFormatter))

    def test_nonlinear_factor_graph(self):
        """Checks that printing NonlinearFactorGraph uses the GTDKeyFormatter"""
        fg = gtsam.NonlinearFactorGraph()
        fg.push_back(gtd.MinTorqueFactor(gtd.internal.TorqueKey(0, 0).key(),
                                         gtsam.noiseModel.Unit.Create(1)))
        self.assertIn('T(0)0', fg.__repr__("nfg: ", gtd.GTDKeyFormatter))

    def test_key_formatter(self):
        """Tests print_ method with various key formatters"""
        torqueKey = gtd.internal.TorqueKey(0, 0).key()
        factor = gtd.MinTorqueFactor(torqueKey, gtsam.noiseModel.Unit.Create(1))
        # test GTDKeyFormatter
        with patch('sys.stdout', new = StringIO()) as fake_out:
            factor.print_('factor: ', gtd.GTDKeyFormatter)
            self.assertIn('factor: min torque factor', fake_out.getvalue())
            self.assertIn('keys = { T(0)0 }', fake_out.getvalue())
        # test functional
        def myKeyFormatter(key):
            return 'this is my key formatter {}'.format(key)
        with patch('sys.stdout', new = StringIO()) as fake_out:
            factor.print_('factor: ', myKeyFormatter)
            self.assertIn('factor: min torque factor', fake_out.getvalue())
            self.assertIn(
                'keys = {{ this is my key formatter {} }}'.format(torqueKey), fake_out.getvalue())
        # test lambda
        with patch('sys.stdout', new = StringIO()) as fake_out:
            factor.print_('factor: ', lambda key: 'lambda formatter {}'.format(key))
            self.assertIn('factor: min torque factor', fake_out.getvalue())
            self.assertIn(
                'keys = {{ lambda formatter {} }}'.format(torqueKey), fake_out.getvalue())

if __name__ == "__main__":
    unittest.main()
