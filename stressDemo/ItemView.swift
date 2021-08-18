//
//  ItemView.swift
//  stressDemo
//
//  Created by Prasenjit on 16/8/2021.
//

import Foundation
import UIKit

@IBDesignable
class itemView: UIView {
    @IBInspectable var shadowColor: UIColor?  {
        set {
            guard let uiColor = newValue else {return}
            self.layer.shadowColor = uiColor.cgColor
        }
        get {
            guard let color = layer.shadowColor else {
                return nil
            }
            return UIColor(cgColor: color)
        }
    }

    @IBInspectable var shadowRadius: CGFloat {
        set {
            self.layer.shadowRadius = newValue
        }
        get {
            return self.layer.shadowRadius
        }
    }

    @IBInspectable var shadowOpacity: Float {
        set {
            self.layer.shadowOpacity = newValue
        }
        get {
            return self.layer.shadowOpacity
        }
    }
    @IBInspectable var shadowOffset: CGSize {
        set {
            self.layer.shadowOffset = newValue
        }
        get {
            return self.layer.shadowOffset
        }
    }
    @IBInspectable var cornerRadius: CGFloat {
        set {
            self.layer.cornerRadius = newValue
        }
        get {
            return self.layer.cornerRadius
        }
    }
}

