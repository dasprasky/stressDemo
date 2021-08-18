//
//  ViewController.swift
//  stressDemo
//
//  Created by Prasenjit on 16/8/2021.
//

import UIKit
import Lottie

class ViewController: UIViewController {
    
    var scantimer: Timer!
    var scanTime = 30
    
    // 1. Create the AnimationView
    @IBOutlet weak var connetingText: UILabel!
    private var animationView: AnimationView?
    @IBOutlet weak var connect_button: UIButton!
    @IBAction func connect(_ sender: Any) {
        connetingText.isHidden = false
        BleSingleton.shared.startCentralManager()

        connect_button.isHidden = true
        animationView?.isHidden = false
        // 6. Play animation
        animationView!.play()
    
    scantimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true){ timer in
        self.scanTime-=self.scanTime
        if self.scanTime <= 0 {
            self.scantimer.invalidate()
            self.scanTime = 30
            BleSingleton.shared.completeScanning()
            
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
                self.animationView!.stop()
                self.animationView?.isHidden = true
                self.connect_button.isHidden = false
                self.connetingText.isHidden = true
                self.performSegue(withIdentifier: "connectSegue", sender: self)
            }
        }}
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        connetingText.isHidden = true
        // Do any additional setup after loading the view.
        // 2. Start AnimationView with animation name (without extension)
        animationView = .init(name: "ble_animation")
        animationView!.frame = view.bounds
        // 3. Set animation content mode
        animationView!.contentMode = .scaleAspectFit
        // 4. Set animation loop mode
        animationView!.loopMode = .loop
        // 5. Adjust animation speed
        animationView!.animationSpeed = 0.75
        view.addSubview(animationView!)
        animationView?.isHidden = true
        
                
    }

}
@IBDesignable extension UIButton {

    @IBInspectable var borderWidth: CGFloat {
        set {
            layer.borderWidth = newValue
        }
        get {
            return layer.borderWidth
        }
    }

    @IBInspectable var cornerRadius: CGFloat {
        set {
            layer.cornerRadius = newValue
        }
        get {
            return layer.cornerRadius
        }
    }

    @IBInspectable var borderColor: UIColor? {
        set {
            guard let uiColor = newValue else { return }
            layer.borderColor = uiColor.cgColor
        }
        get {
            guard let color = layer.borderColor else { return nil }
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
}

