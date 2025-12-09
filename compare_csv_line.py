import csv
import os

def compare_csv_files(file_path1, file_path2):
    # ファイルが存在するか確認
    if not os.path.exists(file_path1) or not os.path.exists(file_path2):
        print("エラー: 指定されたファイルが見つかりません。パスを確認してください。")
        return

    try:
        # 2つのファイルを開く（Windowsの場合は encoding='shift_jis' などに変更が必要な場合があります）
        with open(file_path1, mode='r', encoding='utf-8', newline='') as f1, \
                open(file_path2, mode='r', encoding='utf-8', newline='') as f2:

            reader1 = csv.reader(f1)
            reader2 = csv.reader(f2)

            print(f"{'行番号':<6} | {'判定':<6} | {'詳細'}")
            print("-" * 50)

            same_count = 0
            differ_count = 0

            # zipを使って2つのファイルを同時に読み込む
            # enumerateで行番号を取得 (1からスタート)
            for i, (row1, row2) in enumerate(zip(reader1, reader2), 1):

                if row1 == row2:
                    result = "同じ"
                    detail = ""
                    same_count += 1
                else:
                    result = "違う"
                    # 違う場合は、それぞれの内容を表示（見やすくするため先頭部分のみ表示）
                    detail = f"File1: {str(row1)[:30]}... / File2: {str(row2)[:30]}..."
                    differ_count += 1

                print(f"{i:<8} | {result:<8} | {detail}")

            # 行数の確認（全403行が表示されたか確認用）
            print("-" * 50)
            print(f"比較終了: 全 {i} 行を処理しました。")
            print(f"同じ行数: {same_count}, 違う行数: {differ_count}")

    except Exception as e:
        print(f"エラーが発生しました: {e}")

# ==========================================
# 設定: ここに2つのCSVファイルのパスを指定してください
# ==========================================
csv_path_A = 'results/verification_reproducibility_gemma3_27B_0_1206.csv'
csv_path_B = 'results/verification_reproducibility_gemma3_27B_1_1206.csv'

# 実行
compare_csv_files(csv_path_A, csv_path_B)