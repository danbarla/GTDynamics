"""
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 *
 * @file  test_link.py
 * @brief Test Link class.
 * @author Frank Dellaert, Varun Agrawal, Mandy Xie, Alejandro Escontrela, and Yetong Zhang
"""

# pylint: disable=no-name-in-module, import-error, no-member
import os.path as osp
import unittest

import numpy as np
from gtsam import Point3, Pose3, Rot3
from gtsam.utils.test_case import GtsamTestCase

import gtdynamics as gtd


class TestCollocation(GtsamTestCase):

    def test_collocation(self):
        collocation = gtd.DynamicsGraph.CollocationScheme.Trapezoidal
        print(collocation)


if __name__ == "__main__":
    unittest.main()
