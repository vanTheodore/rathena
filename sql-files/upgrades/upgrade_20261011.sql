-- Convert the engine from MyISAM to InnoDB
ALTER TABLE `login` ENGINE = InnoDB;
ALTER TABLE `global_acc_reg_num` ENGINE = InnoDB;
ALTER TABLE `global_acc_reg_str` ENGINE = InnoDB;
